// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include "rmb.h"
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <errno.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#include <algorithm>  // std::sort

#include <limits>

#include "../../rados-cluster.h"
#include "../../rados-cluster-impl.h"
#include "../../rados-storage.h"
#include "../../rados-storage-impl.h"
#include "../../rados-metadata-storage-ima.h"
#include "../../rados-metadata-storage-module.h"
#include "rados-mail-object.h"
#include "ls_cmd_parser.h"
#include "mailbox_tools.h"
#include "rados-util.h"
#include "rados-namespace-manager.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "rados-metadata-storage-default.h"

static void argv_to_vec(int argc, const char **argv, std::vector<const char *> *args) {
  args->insert(args->end(), argv + 1, argv + argc);
}

static void dashes_to_underscores(const char *input, char *output) {
  char c = 0;
  char *o = output;
  const char *i = input;
  // first two characters are copied as-is
  *o = *i++;
  if (*o++ == '\0')
    return;
  *o = *i++;
  if (*o++ == '\0')
    return;
  for (; ((c = *i)); ++i) {
    if (c == '=') {
      strcpy(o, i);
      return;
    }
    if (c == '-')
      *o++ = '_';
    else
      *o++ = c;
  }
  *o++ = '\0';
}
static bool ceph_argparse_flag(std::vector<const char *> &args, std::vector<const char *>::iterator &i, ...) {
  const char *first = *i;
  char tmp[strlen(first) + 1];
  dashes_to_underscores(first, tmp);
  first = tmp;
  va_list ap;

  va_start(ap, i);
  while (1) {
    const char *a = va_arg(ap, char *);
    if (a == NULL) {
      va_end(ap);
      return false;
    }
    char a2[strlen(a) + 1];
    dashes_to_underscores(a, a2);
    if (strcmp(a2, first) == 0) {
      i = args.erase(i);
      va_end(ap);
      return true;
    }
  }
}
static int va_ceph_argparse_witharg(std::vector<const char *> *args, std::vector<const char *>::iterator *i,
                                    std::string *ret, std::ostream &oss, va_list ap) {
  const char *first = *(*i);
  char tmp[strlen(first) + 1];
  dashes_to_underscores(first, tmp);
  first = tmp;

  // does this argument match any of the possibilities?
  while (1) {
    const char *a = va_arg(ap, char *);
    if (a == NULL)
      return 0;

    int strlen_a = strlen(a);
    char a2[strlen_a + 1];

    dashes_to_underscores(a, a2);
    if (strncmp(a2, first, strlen(a2)) == 0) {
      if (first[strlen_a] == '=') {
        *ret = first + strlen_a + 1;
        *i = args->erase(*i);
        return 1;
      } else if (first[strlen_a] == '\0') {
        // find second part (or not)
        if (*i + 1 == args->end()) {
          oss << "Option " << *(*i) << " requires an argument." << std::endl;
          *i = args->erase(*i);
          return -EINVAL;
        }
        *i = args->erase(*i);
        *ret = *(*i);
        *i = args->erase(*i);
        return 1;
      }
    }
  }
}

static bool ceph_argparse_witharg(std::vector<const char *> *args, std::vector<const char *>::iterator *i,
                                  std::string *ret, ...) {
  int r;
  va_list ap;
  va_start(ap, ret);
  r = va_ceph_argparse_witharg(args, i, ret, std::cerr, ap);
  va_end(ap);
  if (r < 0)
    _exit(1);
  return r != 0;
}

/** Once we see a standalone double dash, '--', we should remove it and stop
 * looking for any other options and flags. */
static bool ceph_argparse_double_dash(std::vector<const char *> *args, std::vector<const char *>::iterator *i) {
  if (strcmp(*(*i), "--") == 0) {
    *i = args->erase(*i);
    return true;
  }
  return false;
}

static void usage(std::ostream &out) {
  out << "usage: rmb [options] [commands]\n"
         "   -p    ceph mail storage pool, default:'mail_storage'\n"
         "   -N    namespace e.g. dovecot user name\n"
         "         specify the namespace/user\n"
         "   -O    path to store the boxes, default: '$HOME/rmb'\n"
         "   -h    print this information\n"
         "   -o    defines the dovecot-ceph configuration object\n"
         "   -c    rados cluster name, default: 'ceph'\n"
         "   -u    rados user name, default: 'client.admin' \n"
         "\n"
         "\nMAIL COMMANDS\n"
         "    ls -    list all mails and mailbox statistic\n"
         "            use Metadata value to filter results \n"
         "            filter: e.g. U=7, \"U<7\", \"U>7\"\n"
         "            date format:  %Y-%m-%d %H:%M:%S e.g. (\"R=2017-08-22 14:30\")\n"
         "            comparison operators: =,>,< for strings only = is supported.\n"
         "    get -   download mails to file\n"
         "            filter: e.g. U=7, \"U<7\", \"U>7\"\n"
         "            date format: %Y-%m-%d %H:%M:%S e.g. (\"R=2017-08-22 14:30\")\n"
         "            comparison operators: =,>,< for strings only = is supported.\n"
         "    set     oid metadata value   e.g. U 1 B INBOX R \"2017-08-22 14:30\"\n"
         "    sort    values: uid, recv_date, save_date, phy_size\n"
         "    lspools list all available pools\n"
         "\n"
         "    delete  deletes the ceph object, use oid attribute to identify mail.\n"
         "    rename  dovecot_user_name, rename a user\n"
         "    lspools list pools\n"

         "\nMAILBOX COMMANDS\n"
         "    ls     mb  -N user        list all mailboxes\n"
         "\nCONFIGURATION COMMANDS\n"
         "    cfg create            create the default configuration\n"
         "    cfg show              print configuration to screen\n"
         "    cfg update key=value  sets the configuration value key=value\n"
         "                          e.g. user_mapping=true\n"
         "\n";
}

static void usage_exit() {
  usage(std::cerr);
  exit(1);
}

static void query_mail_storage(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::CmdLineParser *parser,
                               bool download, librmb::RadosStorage *storage) {
  std::map<std::string, librmb::RadosMailBox *> mailbox;

  for (std::vector<librmb::RadosMailObject *>::iterator it = mail_objects->begin(); it != mail_objects->end(); ++it) {
    std::string mailbox_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_MAILBOX_GUID));
    std::string mailbox_guid = (*it)->get_metadata(mailbox_key);
    std::string mailbox_orig_name_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_ORIG_MAILBOX));
    std::string mailbox_orig_name = (*it)->get_metadata(mailbox_orig_name_key);

    if (parser->contains_key(mailbox_key)) {
      librmb::Predicate *p = parser->get_predicate(mailbox_key);
      if (!p->eval(mailbox_guid)) {
        continue;
      }
    }
    if (mailbox.count(mailbox_guid) > 0) {
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_mail_size());
    } else {
      mailbox[mailbox_guid] = new librmb::RadosMailBox(mailbox_guid, 1, mailbox_orig_name);
      mailbox[mailbox_guid]->set_xattr_filter(parser);
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_mail_size());
    }
  }
  std::cout << "mailbox_count: " << mailbox.size() << std::endl;

  {
    for (std::map<std::string, librmb::RadosMailBox *>::iterator it = mailbox.begin(); it != mailbox.end(); ++it) {
      if (it->second->get_mail_count() == 0) {
        continue;
      }

      std::cout << it->second->to_string() << std::endl;

      if (download) {
        librmb::MailboxTools tools(it->second, parser->get_output_dir());
        if (tools.init_mailbox_dir() < 0) {
          std::cout << " error initializing output dir : " << parser->get_output_dir() << std::endl;
          break;
        }

        for (std::vector<librmb::RadosMailObject *>::iterator it_mail = it->second->get_mails().begin();
             it_mail != it->second->get_mails().end(); ++it_mail) {
          const std::string oid = (*it_mail)->get_oid();

          time_t pm_time;
          uint64_t size_r = 0;
          storage->stat_mail(oid, &size_r, &pm_time);

          (*it_mail)->set_mail_size(size_r);
          int read = storage->read_mail(oid, (*it_mail)->get_mail_buffer());
          if (read > 0) {
            if (tools.save_mail((*it_mail)) < 0) {
              std::cout << " error saving mail : " << oid << " to " << tools.get_mailbox_path() << std::endl;
            }
          }
        }
      }
    }
  }

  for (auto &it : mailbox) {
    delete it.second;
  }
}

static void release_exit(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::RadosCluster *cluster,
                         bool show_usage) {
  for (auto mo : *mail_objects) {
    delete mo;
  }
  cluster->deinit();
  if (show_usage == true) {
    usage_exit();
  }
}

static bool sort_uid(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_MAIL_UID);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_MAIL_UID), &sz);
  return i_uid < j_uid;
}

static bool sort_recv_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME), &sz);
  return i_uid < j_uid;
}

static bool sort_phy_size(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE), &sz);
  return i_uid < j_uid;
}

static bool sort_save_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  return *i->get_rados_save_date() < *j->get_rados_save_date();
}

static void load_objects(librmb::RadosStorageMetadataModule *ms, librmb::RadosStorageImpl &storage,
                         std::vector<librmb::RadosMailObject *> &mail_objects, std::string &sort_string) {
  // get load all objects metadata into memory
  librados::NObjectIterator iter(storage.get_io_ctx().nobjects_begin());
  while (iter != storage.get_io_ctx().nobjects_end()) {
    librmb::RadosMailObject *mail = new librmb::RadosMailObject();
    std::string oid = iter->get_oid();
    mail->set_oid(oid);
    ms->load_metadata(mail);

    if (mail->get_metadata()->size() == 0) {
      std::cout << " pool object " << oid << " is not a mail object" << std::endl;
      ++iter;
      continue;
    }
    uint64_t object_size = 0;
    time_t save_date_rados = 0;
    storage.stat_mail(iter->get_oid(), &object_size, &save_date_rados);

    mail->set_mail_size(object_size);
    mail->set_rados_save_date(save_date_rados);
    ++iter;
    mail_objects.push_back(mail);
  }

  if (sort_string.compare("uid") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_uid);
  } else if (sort_string.compare("recv_date") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_recv_date);
  } else if (sort_string.compare("phy_size") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_phy_size);
  } else {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_save_date);
  }
}

static void parse_cmd_line_args(std::map<std::string, std::string> *opts, bool &is_config,
                                std::map<std::string, std::string> *metadata, std::vector<const char *> *args,
                                bool &create_config, bool &show_usage, bool &update_confirmed) {
  std::vector<const char *>::iterator i;
  std::string val;
  unsigned int idx = 0;

  for (i = (*args).begin(); i != (*args).end();) {
    if (ceph_argparse_double_dash(args, &i)) {
      break;
    } else if (ceph_argparse_witharg(args, &i, &val, "-p", "--pool", static_cast<char>(NULL))) {
      (*opts)["pool"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-N", "--namespace", static_cast<char>(NULL))) {
      (*opts)["namespace"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-O", "--out", static_cast<char>(NULL))) {
      (*opts)["out"] = val;
    } else if (ceph_argparse_flag(*args, i, "-h", "--help", (char *)(NULL))) {
      show_usage = true;
    } else if (ceph_argparse_witharg(args, &i, &val, "-o", "--object", static_cast<char>(NULL))) {
      (*opts)["cfg_obj"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-c", "--cluster", static_cast<char>(NULL))) {
      (*opts)["clustername"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-u", "--rados_user", static_cast<char>(NULL))) {
      (*opts)["rados_user"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "ls", "--ls", static_cast<char>(NULL))) {
      (*opts)["ls"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "get", "--get", static_cast<char>(NULL))) {
      (*opts)["get"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "set", "--set", static_cast<char>(NULL))) {
      (*opts)["set"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "sort", "--sort", static_cast<char>(NULL))) {
      (*opts)["sort"] = val;
    } else if (ceph_argparse_flag(*args, i, "cfg", "--config", (char *)(NULL))) {
      is_config = true;
    } else if (ceph_argparse_witharg(args, &i, &val, "update", "--update", static_cast<char>(NULL))) {
      (*opts)["update"] = val;
    } else if (ceph_argparse_flag(*args, i, "create", "--create", (char *)(NULL))) {
      create_config = true;
    } else if (ceph_argparse_flag(*args, i, "show", "--show", (char *)(NULL))) {
      (*opts)["print_cfg"] = "true";
    } else if (ceph_argparse_flag(*args, i, "-yes-i-really-really-mean-it", "--yes-i-really-really-mean-it",
                                  (char *)(NULL))) {
      update_confirmed = true;
    } else if (ceph_argparse_witharg(args, &i, &val, "delete", "--delete", static_cast<char>(NULL))) {
      // delete oid
      (*opts)["to_delete"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "rename", "--rename", static_cast<char>(NULL))) {
      // rename
      (*opts)["to_rename"] = val;
    } else {
      if (idx + 1 < (*args).size()) {
        std::string m_idx((*args)[idx]);
        (*metadata)[m_idx] = std::string((*args)[idx + 1]);
        idx++;
      }
      ++idx;
      ++i;
    }
  }
}

int cmd_lspools() {
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);
  cluster.init();
  if (cluster.connect() < 0) {
    std::cout << " error opening rados connection" << std::endl;
  } else {
    std::list<std::string> vec;
    int ret = cluster.get_cluster().pool_list(vec);
    if (ret == 0) {
      for (std::list<std::string>::iterator it = vec.begin(); it != vec.end(); ++it) {
        std::cout << ' ' << *it << std::endl;
      }
    }
  }
  cluster.deinit();
  return 0;
}

static void cmd_config_option(bool is_config_option, bool create_config, const std::string &obj_, bool confirmed,
                                 std::map<std::string, std::string> &opts, librmb::RadosClusterImpl &cluster,
                                 librmb::RadosCephConfig &ceph_cfg) {
  if (is_config_option) {
    bool has_update = opts.find("update") != opts.end();
    bool has_ls = opts.find("print_cfg") != opts.end();
    if (has_update && has_ls) {
      usage_exit();
    }
    if (create_config) {
      std::cout << "Error: there already exists a configuration " << obj_ << std::endl;
      cluster.deinit();
      // return -1;
      exit(-1);
    }
    if (has_ls) {
      std::cout << ceph_cfg.get_config()->to_string() << std::endl;
    } else if (has_update) {
      std::size_t key_val_separator_idx = opts["update"].find("=");
      if (key_val_separator_idx != std::string::npos) {
        std::string key = opts["update"].substr(0, key_val_separator_idx);
        std::string key_val = opts["update"].substr(key_val_separator_idx + 1, opts["update"].length() - 1);
        bool failed = false;
        if (!confirmed) {
          std::cout << "WARNING:" << std::endl;
          std::cout << "Changing this setting, after e-mails have been stored, could lead to a situation in which "
                       "users can no longer access their e-mail!!!"
                    << std::endl;
          std::cout << "To confirm pass --yes-i-really-really-mean-it " << std::endl;
        } else {
          if (ceph_cfg.is_valid_key_value(key, key_val)) {
            failed = !ceph_cfg.update_valid_key_value(key, key_val);
            // std::cout << " saving : " << failed << std::endl;
          } else {
            failed = true;
            std::cout << "Error: key : " << key << " value: " << key_val << " is not valid !" << std::endl;
            if (key_val.compare("TRUE") == 0 || key_val.compare("FALSE") == 0) {
              std::cout << "Error: value: TRUE|FALSE not supported use lower case! " << std::endl;
            }
          }
          if (!failed) {
            if (ceph_cfg.save_cfg() != 0) {
              std::cout << " saving cfg failed" << std::endl;
            } else {
              std::cout << " saving cfg" << std::endl;
            }
          }
        }
      }
    }

    cluster.deinit();
    exit(0);
  }
}

static void cmd_delete_mail(bool delete_mail_option, bool confirmed, std::map<std::string, std::string> &opts,
                               librmb::RadosStorageImpl &storage, librmb::RadosClusterImpl &cluster) {
  if (delete_mail_option) {
    if (!confirmed) {
      std::cout << "WARNING: Deleting a mail object will remove the object from ceph, but not from dovecot index, this "
                   "may lead to corrupt mailbox\n"
                << " add --yes-i-really-really-mean-it to confirm the delete " << std::endl;
    } else {
      if (storage.delete_mail(opts["to_delete"]) == 0) {
        std::cout << "unable to delete e-mail object with oid: " << opts["to_delete"] << std::endl;
      } else {
        std::cout << "Success: email object with oid: " << opts["to_delete"] << " deleted" << std::endl;
      }
    }
    cluster.deinit();
    exit(0);
  }
}

static void cmd_rename_user(bool rename_user_option, librmb::RadosDovecotCephCfgImpl cfg, bool confirmed,
                               const std::string &uid, std::map<std::string, std::string> &opts,
                               librmb::RadosClusterImpl &cluster, librmb::RadosStorageImpl &storage) {
  if (rename_user_option) {
    if (!cfg.is_user_mapping()) {
      std::cout << "Error: The configuration option generate_namespace needs to be active, to be able to rename a user"
                << std::endl;
      cluster.deinit();
      exit(0);
    }
    if (!confirmed) {
      std::cout << "WARNING: renaming a user may lead to data loss! Do you really really want to do this? \n add "
                   "--yes-i-really-really-mean-it to confirm "
                << std::endl;
      cluster.deinit();
      exit(0);
    }
    std::string src_ = uid + cfg.get_user_suffix();

    std::string dest_ = opts["to_rename"] + cfg.get_user_suffix();
    if (src_.compare(dest_) == 0) {
      std::cout << "Error: you need to give a valid username not equal to -N" << std::endl;
      cluster.deinit();
      exit(0);
    }
    std::list<librmb::RadosMetadata> list;
    std::cout << " copy namespace configuration src " << src_ << " to dest " << dest_ << " in namespace "
              << cfg.get_user_ns() << std::endl;
    storage.set_namespace(cfg.get_user_ns());
    uint64_t size;
    time_t save_time;
    int exist = storage.stat_mail(src_, &size, &save_time);
    if (exist < 0) {
      std::cout << "Error there does not exist a configuration file for " << src_ << std::endl;
      cluster.deinit();
      exit(0);
    }
    exist = storage.stat_mail(dest_, &size, &save_time);
    if (exist >= 0) {
      std::cout << "Error: there already exists a configuration file: " << dest_ << std::endl;
      cluster.deinit();
      exit(0);
    }
    if (storage.copy(src_, cfg.get_user_ns().c_str(), dest_, cfg.get_user_ns().c_str(), list)) {
      if (storage.delete_mail(src_) != 0) {
        std::cout << "Error removing " << src_ << std::endl;
      }
    } else {
      std::cout << "Error renaming " << src_ << std::endl;
    }
    cluster.deinit();
    exit(0);
  }
}

int main(int argc, const char **argv) {
  std::vector<librmb::RadosMailObject *> mail_objects;
  std::vector<const char *> args;

  std::map<std::string, std::string> opts;
  std::map<std::string, std::string> metadata;
  std::string sort_type;

  bool is_config_option = false;
  bool create_config = false;
  bool confirmed = false;
  bool show_usage = false;
  bool is_lspools_cmd = false;
  bool delete_mail_option = false;
  bool rename_user_option = false;
  std::string config_obj = "obj";
  std::string rados_user = "client.admin";
  std::string rados_cluster;

  argv_to_vec(argc, argv, &args);

  parse_cmd_line_args(&opts, is_config_option, &metadata, &args, create_config, show_usage, confirmed);

  if (show_usage) {
    usage_exit();
  }

  if (args.size() <= 0 && opts.size() <= 0 && !is_config_option) {
    usage_exit();
  }

  is_lspools_cmd = strcmp(args[0], "lspools") == 0;
  delete_mail_option = opts.find("to_delete") != opts.end();
  sort_type = (opts.find("sort") != opts.end()) ? opts["sort"] : "uid";
  rename_user_option = opts.find("to_rename") != opts.end() ? true : false;
  rados_cluster = (opts.find("clustername") != opts.end()) ? opts["clustername"] : "ceph";
  rados_user = (opts.find("rados_user") != opts.end()) ? opts["rados_user"] : "client.admin";
  // set pool to default or given pool name
  std::string pool_name(opts.find("pool") == opts.end() ? "mail_storage" : opts["pool"]);

  if (is_lspools_cmd) {
    return cmd_lspools();
  }

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);
  int open_connection = storage.open_connection(pool_name, rados_cluster, rados_user);
  if (open_connection < 0) {
    std::cout << " error opening rados connection. Errorcode: " << open_connection << std::endl;
    cluster.deinit();
    return -1;
  }

  // initialize configuration
  librmb::RadosCephConfig ceph_cfg(&storage.get_io_ctx());
  // set config object
  config_obj = opts.find("cfg_obj") != opts.end() ? opts["cfg_obj"] : ceph_cfg.get_cfg_object_name();
  ceph_cfg.set_cfg_object_name(config_obj);

  int ret_load_cfg = ceph_cfg.load_cfg();
  if (ret_load_cfg < 0) {
    if (create_config) {
      if (ceph_cfg.save_cfg() < 0) {
        std::cout << "loading config object failed " << std::endl;
      }

    } else if (ret_load_cfg == -ENOENT) {
      std::cout << "dovecot-ceph config does not exist, use -C option to create the default config" << std::endl;
    } else {
      std::cout << "unknown error, unable to read dovecot-ceph config errorcode : " << ret_load_cfg << std::endl;
    }
    cluster.deinit();
    exit(0);
  }

  cmd_config_option(is_config_option, create_config, config_obj, confirmed, opts, cluster, ceph_cfg);

  librmb::RadosConfig dovecot_cfg;
  dovecot_cfg.set_config_valid(true);
  librmb::RadosDovecotCephCfgImpl cfg(dovecot_cfg, ceph_cfg);
  librmb::RadosNamespaceManager mgr(&cfg);

  librmb::RadosStorageMetadataModule *ms;
  // decide metadata storage!
  std::string storage_module_name = ceph_cfg.get_metadata_storage_module();
  if (storage_module_name.compare(librmb::RadosMetadataStorageIma::module_name) == 0) {
    ms = new librmb::RadosMetadataStorageIma(&storage.get_io_ctx(), &cfg);
  } else {
    ms = new librmb::RadosMetadataStorageDefault(&storage.get_io_ctx());
  }

  // namespace (user) needs to be set
  if (opts.find("namespace") == opts.end()) {
    std::cout << "xist hier" << std::endl;
    usage_exit();
  }
  std::string uid(opts["namespace"] + cfg.get_user_suffix());
  std::string ns;
  if (mgr.lookup_key(uid, &ns)) {
    storage.set_namespace(ns);
  } else {
    // use
    if (!mgr.lookup_key(uid, &ns)) {
      std::cout << " error unable to determine namespace" << std::endl;
      return -1;
    }
    storage.set_namespace(ns);
  }

  cmd_delete_mail(delete_mail_option, confirmed, opts, storage, cluster);

  cmd_rename_user(rename_user_option, cfg, confirmed, uid, opts, cluster, storage);
  if (opts.find("ls") != opts.end()) {
    librmb::CmdLineParser parser(opts["ls"]);
    if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
      load_objects(ms, storage, mail_objects, sort_type);
      query_mail_storage(&mail_objects, &parser, false, nullptr);
    } else {
      // tear down.
      release_exit(&mail_objects, &cluster, true);
    }
  } else if (opts.find("get") != opts.end()) {
    librmb::CmdLineParser parser(opts["get"]);

    if (opts.find("out") != opts.end()) {
      parser.set_output_dir(opts["out"]);
    } else {
      char outpath[PATH_MAX];
      char *home = getenv("HOME");
      if (home != NULL) {
        snprintf(outpath, sizeof(outpath), "%s/rmb", home);
      } else {
        snprintf(outpath, sizeof(outpath), "rmb");
      }
      parser.set_output_dir(outpath);
    }

    if (opts["get"].compare("all") == 0 || opts["get"].compare("-") == 0 || parser.parse_ls_string()) {
      // get load all objects metadata into memory
      load_objects(ms, storage, mail_objects, sort_type);
      query_mail_storage(&mail_objects, &parser, true, &storage);
    } else {
      // tear down.
      release_exit(&mail_objects, &cluster, true);
    }
  } else if (opts.find("set") != opts.end()) {
    std::string oid = opts["set"];
    if (oid.empty() || metadata.size() < 1) {
      release_exit(&mail_objects, &cluster, true);
    }

    for (std::map<std::string, std::string>::iterator it = metadata.begin(); it != metadata.end(); ++it) {
      std::cout << oid << "=> " << it->first << " = " << it->second << '\n';
      librmb::rbox_metadata_key ke = static_cast<librmb::rbox_metadata_key>(it->first[0]);
      std::string value = it->second;
      if (librmb::RadosUtils::is_date_attribute(ke)) {
        if (!librmb::RadosUtils::is_numeric(value)) {
          std::string date;
          if (librmb::RadosUtils::convert_string_to_date(value, &date)) {
            value = date;
          }
        }
      }
      librmb::RadosMailObject obj;
      obj.set_oid(oid);
      ms->load_metadata(&obj);
      librmb::RadosMetadata attr(ke, value);
      ms->set_metadata(&obj, attr);
    }
  }
  // tear down.
  release_exit(&mail_objects, &cluster, false);
}

