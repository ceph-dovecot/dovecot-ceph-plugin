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

#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <errno.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#include <limits>

#include "../../rados-cluster.h"
#include "../../rados-cluster-impl.h"
#include "../../rados-mail.h"
#include "../../rados-storage.h"
#include "../../rados-storage-impl.h"
#include "../../rados-metadata-storage-ima.h"
#include "../../rados-metadata-storage-module.h"
#include "ls_cmd_parser.h"
#include "mailbox_tools.h"
#include "rados-util.h"
#include "rados-namespace-manager.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "rados-metadata-storage-default.h"
#include "rmb-commands.h"

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
         "   -D    debug output \n"
         "   -r    save log with objects to delete => deletes all entries (save,mv,cp) from object store, use with "
         "care!!!! \n "
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

         "\nMAILBOX COMMANDS\n"
         "    ls     mb  -N user        list all mailboxes\n"
         "\nCONFIGURATION COMMANDS\n"
         "    cfg create            create the default configuration\n"
         "    cfg show              print configuration to screen\n"
         "    cfg update key=value  sets the configuration value key=value\n"
         "                          e.g. user_mapping=true\n"
         "\n";
}

__attribute__((noreturn)) static void usage_exit() {
  usage(std::cerr);
  exit(1);
}

static void release_exit(std::vector<librmb::RadosMail *> *mail_objects, librmb::RadosCluster *cluster,
                         bool show_usage) {
  if (mail_objects != nullptr) {
    for (auto mo : *mail_objects) {
      delete mo;
    }
  }
  cluster->deinit();
  if (show_usage == true) {
    usage_exit();
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
    } else if (ceph_argparse_flag(*args, i, "-h", "--help", static_cast<char>(NULL))) {
      show_usage = true;
    } else if (ceph_argparse_witharg(args, &i, &val, "-o", "--object", static_cast<char>(NULL))) {
      (*opts)["cfg_obj"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-c", "--cluster", static_cast<char>(NULL))) {
      (*opts)["clustername"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "-u", "--rados_user", static_cast<char>(NULL))) {
      (*opts)["rados_user"] = val;
    } else if (ceph_argparse_flag(*args, i, "-D", "--debug", static_cast<char>(NULL))) {
      (*opts)["debug"] = "true";
    } else if (ceph_argparse_witharg(args, &i, &val, "-r", "--remove", static_cast<char>(NULL))) {
      (*opts)["remove_save_log"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "ls", "--ls", static_cast<char>(NULL))) {
      (*opts)["ls"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "get", "--get", static_cast<char>(NULL))) {
      (*opts)["get"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "set", "--set", static_cast<char>(NULL))) {
      (*opts)["set"] = val;
    } else if (ceph_argparse_witharg(args, &i, &val, "sort", "--sort", static_cast<char>(NULL))) {
      (*opts)["sort"] = val;
    } else if (ceph_argparse_flag(*args, i, "cfg", "--config", static_cast<char>(NULL))) {
      is_config = true;
    } else if (ceph_argparse_witharg(args, &i, &val, "update", "--update", static_cast<char>(NULL))) {
      (*opts)["update"] = val;
    } else if (ceph_argparse_flag(*args, i, "create", "--create", static_cast<char>(NULL))) {
      create_config = true;
    } else if (ceph_argparse_flag(*args, i, "show", "--show", static_cast<char>(NULL))) {
      (*opts)["print_cfg"] = "true";
    } else if (ceph_argparse_flag(*args, i, "-yes-i-really-really-mean-it", "--yes-i-really-really-mean-it",
                                  static_cast<char>(NULL))) {
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

int main(int argc, const char **argv) {
  std::vector<librmb::RadosMail *> mail_objects;
  std::vector<const char *> args;

  std::map<std::string, std::string> opts;
  std::map<std::string, std::string> metadata;
  std::string sort_type;
  librmb::RmbCommands *rmb_commands = nullptr;

  bool is_config_option = false;
  bool create_config = false;
  bool confirmed = false;
  bool show_usage = false;
  bool is_lspools_cmd = false;
  bool delete_mail_option = false;
  bool rename_user_option = false;
  std::string remove_save_log;
  std::string config_obj = "obj";
  std::string rados_user = "client.admin";
  std::string rados_cluster;
  argv_to_vec(argc, argv, &args);

  parse_cmd_line_args(&opts, is_config_option, &metadata, &args, create_config, show_usage, confirmed);

  if (show_usage) {
    usage_exit();
  }

  if (args.empty() && opts.empty() && !is_config_option) {
    usage_exit();
  }

  is_lspools_cmd = strcmp(args[0], "lspools") == 0;
  delete_mail_option = opts.find("to_delete") != opts.end();
  sort_type = (opts.find("sort") != opts.end()) ? opts["sort"] : "uid";
  rename_user_option = opts.find("to_rename") != opts.end() ? true : false;
  rados_cluster = (opts.find("clustername") != opts.end()) ? opts["clustername"] : "ceph";
  rados_user = (opts.find("rados_user") != opts.end()) ? opts["rados_user"] : "client.admin";
  remove_save_log = (opts.find("remove_save_log") != opts.end()) ? opts["remove_save_log"] : "";

  if (!remove_save_log.empty()) {
    if (confirmed) {
      std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;
      return librmb::RmbCommands::delete_with_save_log(remove_save_log, rados_cluster, rados_user, &moved_items);
    } else {
      std::cout << "WARNING:" << std::endl;
      std::cout << "Performing this command, will delete all mail objects from ceph object store which are "
                   "listed in the log file. This operation is Irreversible!!!!!!! Are you sure you want to do this?"
                << std::endl;
      std::cout << "To confirm pass --yes-i-really-really-mean-it " << std::endl;
      return 0;
    }
  }
  // set pool to default or given pool name
  std::string pool_name(opts.find("pool") == opts.end() ? "mail_storage" : opts["pool"]);

  if (is_lspools_cmd) {
    return librmb::RmbCommands::lspools();
  }

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);
  int open_connection = storage.open_connection(pool_name, rados_cluster, rados_user);
  if (open_connection < 0) {
    std::cerr << " error opening rados connection. Errorcode: " << open_connection << std::endl;
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
        std::cerr << "loading config object failed " << std::endl;
      } else {
        std::cout << "config created" << std::endl;
      }

    } else if (ret_load_cfg == -ENOENT) {
      std::cerr << "dovecot-ceph config does not exist, use -C option to create the default config" << std::endl;
    } else {
      std::cerr << "unknown error, unable to read dovecot-ceph config errorcode : " << ret_load_cfg << std::endl;
    }
    cluster.deinit();
    exit(0);
  }

  // connection to rados is established!
  rmb_commands = new librmb::RmbCommands(&storage, &cluster, &opts);
  if (is_config_option) {
    if (rmb_commands->configuration(confirmed, ceph_cfg) < 0) {
      std::cerr << "error processing config option" << std::endl;
    }
    delete rmb_commands;
    // tear down.
    release_exit(nullptr, &cluster, false);
    exit(0);
  }

  // namespace (user) needs to be set
  if (opts.find("namespace") == opts.end()) {
    usage_exit();
  }

  std::string uid;
  // load metadata configuration
  librmb::RadosStorageMetadataModule *ms = rmb_commands->init_metadata_storage_module(ceph_cfg, &uid);
  if (ms == nullptr) {
    /// error exit!
    std::cerr << " Error initializing metadata module " << std::endl;
    delete rmb_commands;
    release_exit(&mail_objects, &cluster, false);
    exit(0);
  }

  if (delete_mail_option) {
    if (opts["to_delete"].size() == 1 && opts["to_delete"].compare("-") == 0) {
      if (rmb_commands->delete_namespace(ms, mail_objects, &ceph_cfg, confirmed) < 0) {
        std::cerr << "error deleting namespace " << std::endl;
        release_exit(&mail_objects, &cluster, false);
      }
    } else {
      if (rmb_commands->delete_mail(confirmed) < 0) {
        std::cerr << "error deleting mail" << std::endl;
      }
    }
    release_exit(&mail_objects, &cluster, false);
    delete rmb_commands;
    delete ms;
    exit(0);

  } else if (rename_user_option) {
    if (rmb_commands->rename_user(&ceph_cfg, confirmed, uid) < 0) {
      std::cerr << "error renaming user" << std::endl;
    }
  } else if (opts.find("ls") != opts.end()) {
    librmb::CmdLineParser parser(opts["ls"]);
    if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
      rmb_commands->load_objects(ms, mail_objects, sort_type);
      rmb_commands->query_mail_storage(&mail_objects, &parser, false, false);
      std::cout << " NOTE: rmb tool does not have access to dovecot index. so all objects are set  <<<   MAIL OBJECT "
                   "HAS NO INDEX REFERENCE <<<< use doveadm rmb ls - instead "
                << std::endl;
    }
  } else if (opts.find("get") != opts.end()) {
    librmb::CmdLineParser parser(opts["get"]);

    rmb_commands->set_output_path(&parser);

    if (opts["get"].compare("all") == 0 || opts["get"].compare("-") == 0 || parser.parse_ls_string()) {
      // get load all objects metadata into memory
      rmb_commands->load_objects(ms, mail_objects, sort_type);
      rmb_commands->query_mail_storage(&mail_objects, &parser, true, false);
    }
  } else if (opts.find("set") != opts.end()) {
    rmb_commands->update_attributes(ms, &metadata);
  }

  delete rmb_commands;
  delete ms;

  // tear down.
  release_exit(&mail_objects, &cluster, false);
}
