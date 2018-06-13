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

#include "rmb-commands.h"
#include <algorithm>  // std::sort
#include <cstdio>

#include "../../rados-cluster-impl.h"
#include "../../rados-storage-impl.h"
#include "rados-util.h"
#include "rados-dovecot-config.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "rados-namespace-manager.h"
#include "rados-metadata-storage-ima.h"
#include "rados-metadata-storage-default.h"
#include "ls_cmd_parser.h"

namespace librmb {

RmbCommands::RmbCommands(librmb::RadosStorage *storage_, librmb::RadosCluster *cluster_,
                         std::map<std::string, std::string> *opts_) {
  this->storage = storage_;
  this->cluster = cluster_;
  this->opts = opts_;
  if (this->opts != nullptr) {
    is_debug = ((*opts).find("debug") != (*opts).end()) ? true : false;
  }
}
RmbCommands::~RmbCommands() {
}

void RmbCommands::print_debug(const std::string &msg) {
  if (this->is_debug) {
    std::cout << msg << std::endl;
  }
}
int RmbCommands::delete_with_save_log(const std::string &save_log, const std::string &rados_cluster,
                                      const std::string &rados_user,
                                      std::map<std::string, std::list<librmb::RadosSaveLogEntry>> *moved_items) {
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);
  int count = 0;

  /** check content **/
  std::ifstream read(save_log);
  if (!read.is_open()) {
    std::cerr << " path to log file not valid " << std::endl;
    return -1;
  }
  int line_count = 0;
  while (true) {
    line_count++;
    librmb::RadosSaveLogEntry entry;
    read >> entry;
    if (read.eof()) {
      break;
    }
    if (read.fail()) {
      std::cout << "Objectentry at line '" << line_count << "' is not valid: " << std::endl;
      break;
    }

    if (storage.get_pool_name().compare(entry.pool) != 0) {
      // close connection before open a new one.
      // TODO: worst case are alternating pool entries e.g. mail_storage ,
      //       mail_storage_alt.... maybe we should group the entries by pool...
      storage.close_connection();
      int open_connection = storage.open_connection(entry.pool, rados_cluster, rados_user);
      if (open_connection < 0) {
        std::cerr << " error opening rados connection. Errorcode: " << open_connection << std::endl;
        cluster.deinit();
        return -1;
      }
    }
    storage.set_namespace(entry.ns);
    if (entry.op.compare("save") == 0 || entry.op.compare("cpy") == 0) {
      int ret_delete = storage.delete_mail(entry.oid);
      if (ret_delete < 0) {
        std::cout << "Object " << entry.oid << " not deleted: errorcode: " << ret_delete << std::endl;
      } else {
        std::cout << "Object " << entry.oid << " successfully deleted" << std::endl;
        count++;
      }
    } else {
      int ret = storage.move(entry.oid, entry.ns.c_str(), entry.src_oid, entry.src_ns.c_str(), entry.metadata, true);
      if (ret < 0) {
        std::cerr << "moving : " << entry.oid << " to " << entry.src_oid << " failed ! ret code: " << ret << std::endl;
      } else {
        std::cerr << " ola: mr: " << entry.src_user << std::endl;
        if (moved_items->find(entry.src_user) == moved_items->end()) {
          std::list<librmb::RadosSaveLogEntry> entries;
          entries.push_back(entry);
          (*moved_items)[entry.src_user] = entries;
        } else {
          (*moved_items)[entry.src_user].push_back(entry);
        }
      }
      count++;
    }
  }
  read.close();
  storage.close_connection();
  cluster.deinit();
  return count;
}
// TODO:: currently untestable with mocks.
int RmbCommands::lspools() {
  librmb::RadosClusterImpl cluster;

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

int RmbCommands::delete_mail(bool confirmed) {
  int ret = -1;
  print_debug("entry: delete_mail");
  if (!confirmed) {
    std::cout << "WARNING: Deleting a mail object will remove the object from ceph, but not from dovecot index, this "
                 "may lead to corrupt mailbox\n"
              << " add --yes-i-really-really-mean-it to confirm the delete " << std::endl;
  } else {
    ret = storage->delete_mail((*opts)["to_delete"]);
    if (ret < 0) {
      std::cout << "unable to delete e-mail object with oid: " << (*opts)["to_delete"] << std::endl;
    } else {
      std::cout << "Success: email object with oid: " << (*opts)["to_delete"] << " deleted" << std::endl;
    }
  }
  print_debug("end: delete_mail");
  return ret;
}

int RmbCommands::rename_user(librmb::RadosCephConfig *cfg, bool confirmed, const std::string &uid) {
  print_debug("entry: rename_user");
  if (!cfg->is_user_mapping()) {
    std::cout << "Error: The configuration option generate_namespace needs to be active, to be able to rename a user"
              << std::endl;
    print_debug("end: rename_user");
    return 0;
  }
  if (!confirmed) {
    std::cout << "WARNING: renaming a user may lead to data loss! Do you really really want to do this? \n add "
                 "--yes-i-really-really-mean-it to confirm "
              << std::endl;
    print_debug("end: rename_user");
    return -1;
  }
  std::string src_ = uid + cfg->get_user_suffix();

  std::string dest_ = (*opts)["to_rename"] + cfg->get_user_suffix();
  if (src_.compare(dest_) == 0) {
    std::cout << "Error: you need to give a valid username not equal to -N" << std::endl;
    print_debug("end: rename_user");
    return -1;
  }
  std::list<librmb::RadosMetadata> list;
  std::cout << " copy namespace configuration src " << src_ << " to dest " << dest_ << " in namespace "
            << cfg->get_user_ns() << std::endl;
  storage->set_namespace(cfg->get_user_ns());
  uint64_t size;
  time_t save_time;
  int exist = storage->stat_mail(src_, &size, &save_time);
  if (exist < 0) {
    std::cout << "Error there does not exist a configuration file for " << src_ << std::endl;
    print_debug("end: rename_user");
    return -1;
    }
    exist = storage->stat_mail(dest_, &size, &save_time);
    if (exist >= 0) {
      std::cout << "Error: there already exists a configuration file: " << dest_ << std::endl;
      print_debug("end: rename_user");
      return -1;
    }
    int ret = storage->copy(src_, cfg->get_user_ns().c_str(), dest_, cfg->get_user_ns().c_str(), list);
    if (ret == 0) {
      ret = storage->delete_mail(src_);
      if (ret != 0) {
        std::cout << "Error removing errorcode: " << ret << " oid: " << src_ << std::endl;
      }
    } else {
      std::cout << "Error renaming copy failed: return code:  " << ret << " oid: " << src_ << std::endl;
    }
    print_debug("end: rename_user");
    return ret;
}

int RmbCommands::configuration(bool confirmed, librmb::RadosCephConfig &ceph_cfg) {
  print_debug("entry: configuration");
  bool has_update = (*opts).find("update") != (*opts).end();
  bool has_ls = (*opts).find("print_cfg") != (*opts).end();
  if (has_update && has_ls) {
    std::cerr << "create and ls is not supported, use separately" << std::endl;
    print_debug("end: configuration");
    return -1;
  }

  if (has_ls) {
    std::cout << ceph_cfg.get_config()->to_string() << std::endl;
    print_debug("end: configuration");
    return 0;
  }

  if (!has_update) {
    std::cerr << "create config failed, check parameter" << std::endl;
    print_debug("end: configuration");
    return -1;
  }
  if (!confirmed) {
    std::cout << "WARNING:" << std::endl;
    std::cout << "Changing this setting, after e-mails have been stored, could lead to a situation in which "
                 "users can no longer access their e-mail!!!"
              << std::endl;
    std::cout << "To confirm pass --yes-i-really-really-mean-it " << std::endl;
    print_debug("end: configuration");
    return -1;
  }
  std::size_t key_val_separator_idx = (*opts)["update"].find("=");
  if (key_val_separator_idx == std::string::npos) {
    print_debug("end: configuration");
    return -1;
  }
  std::string key = (*opts)["update"].substr(0, key_val_separator_idx);
  std::string key_val = (*opts)["update"].substr(key_val_separator_idx + 1, (*opts)["update"].length() - 1);
  bool failed = ceph_cfg.update_valid_key_value(key, key_val) ? false : true;
  if (failed) {
    std::cout << "Error: key : " << key << " value: " << key_val << " is not valid !" << std::endl;
    if (key_val.compare("TRUE") == 0 || key_val.compare("FALSE") == 0) {
      std::cout << "Error: value: TRUE|FALSE not supported use lower case! " << std::endl;
    }
    print_debug("end: configuration");
    return -1;
  }

  if (ceph_cfg.save_cfg() < 0) {
    std::cout << " saving cfg failed" << std::endl;

    print_debug("end: configuration");
    return -1;
  }
  std::cout << " saving configuration successful" << std::endl;
  print_debug("end: configuration");

  return 0;
}

bool RmbCommands::sort_uid(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {

  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_MAIL_UID);
  try {
    long i_uid = std::stol(t, &sz);
    long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_MAIL_UID), &sz);
    return i_uid < j_uid;
  } catch (std::exception &e) {
    std::cerr << " sort_uid: " << t << "(" << i->get_oid() << ") or " << j->get_metadata(librmb::RBOX_METADATA_MAIL_UID)
              << " (" << j->get_oid() << ") is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_recv_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME);
  try {
    long i_uid = std::stol(t, &sz);
    long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME), &sz);
    return i_uid < j_uid;
  } catch (std::exception &e) {
    std::cerr << " sort_recv_date: " << t << " or " << j->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME)
              << " is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_phy_size(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {

  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE);
  try {
    long i_uid = std::stol(t, &sz);
    long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE), &sz);
    return i_uid < j_uid;
  } catch (std::exception &e) {
    std::cerr << " sort_physical_size: " << t << " or " << j->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE)
              << " is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_save_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  return *i->get_rados_save_date() < *j->get_rados_save_date();
}

int RmbCommands::load_objects(librmb::RadosStorageMetadataModule *ms,
                              std::vector<librmb::RadosMailObject *> &mail_objects, std::string &sort_string) {
  print_debug("entry: load_objects");
  if (ms == nullptr || storage == nullptr) {
    print_debug("end: load_objects");
    return -1;
  }
  // get load all objects metadata into memory
  librados::NObjectIterator iter(storage->find_mails(nullptr));
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    librmb::RadosMailObject *mail = new librmb::RadosMailObject();
    std::string oid = iter->get_oid();
    uint64_t object_size = 0;
    time_t save_date_rados;
    int ret = storage->stat_mail(oid, &object_size, &save_date_rados);
    if (ret != 0 || object_size == 0) {
      std::cout << " object '" << oid << "' is not a valid mail object, size = 0" << std::endl;
      ++iter;
      delete mail;
      continue;
    }
    mail->set_oid(oid);
    if (ms->load_metadata(mail) < 0) {
      std::cout << " loading metadata of object '" << oid << "' faild " << std::endl;
      ++iter;
      delete mail;
      continue;
    }

    if (mail->get_metadata()->empty()) {
      std::cout << " pool object " << oid << " is not a mail object" << std::endl;
      ++iter;
      delete mail;
      continue;
    }

    if (!librmb::RadosUtils::validate_metadata(mail->get_metadata())) {
      std::cout << "object : " << oid << " metadata is not valid " << std::endl;
      ++iter;
      delete mail;
      continue;
    }

    mail->set_mail_size(object_size);
    mail->set_rados_save_date(save_date_rados);
    ++iter;
    mail_objects.push_back(mail);
    if (is_debug) {
      std::cout << "added: mail " << mail->get_oid() << std::endl;
    }
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

  print_debug("end: load_objects");
  return 0;
}

int RmbCommands::print_mail(std::map<std::string, librmb::RadosMailBox *> *mailbox, std::string &output_dir,
                            bool download) {
  print_debug("entry:: print_mail");

  for (std::map<std::string, librmb::RadosMailBox *>::iterator it = mailbox->begin(); it != mailbox->end(); ++it) {
    if (it->second->get_mail_count() == 0) {
      continue;
    }
    std::cout << it->second->to_string() << std::endl;
    if (!download) {
      continue;
    }

    librmb::MailboxTools tools(it->second, output_dir);
    if (tools.init_mailbox_dir() < 0) {
      std::cout << " error initializing output dir : " << output_dir << std::endl;
      break;
    }
    for (std::vector<librmb::RadosMailObject *>::iterator it_mail = it->second->get_mails().begin();
         it_mail != it->second->get_mails().end(); ++it_mail) {
      const std::string oid = (*it_mail)->get_oid();

      if (storage->read_mail(oid, (*it_mail)->get_mail_buffer()) > 0) {
        if (tools.save_mail((*it_mail)) < 0) {
          std::cout << " error saving mail : " << oid << " to " << tools.get_mailbox_path() << std::endl;
        }
      }
    }
  }

  print_debug("end: print_mail");

  return 0;
}

int RmbCommands::query_mail_storage(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::CmdLineParser *parser,
                                    bool download) {
  print_debug("entry: query_mail_storage");
  int ret = 0;
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

  ret = print_mail(&mailbox, parser->get_output_dir(), download);

  for (auto &it : mailbox) {
    delete it.second;
  }
  print_debug("end: query_mail_storage");
  return ret;
}

RadosStorageMetadataModule *RmbCommands::init_metadata_storage_module(librmb::RadosCephConfig &ceph_cfg,
                                                                      std::string *uid) {
  print_debug("entry: init_metadata_storage_module");
  librmb::RadosConfig dovecot_cfg;
  RadosStorageMetadataModule *ms;
  dovecot_cfg.set_config_valid(true);
  ceph_cfg.set_config_valid(true);
  librmb::RadosDovecotCephCfgImpl cfg(dovecot_cfg, ceph_cfg);
  librmb::RadosNamespaceManager mgr(&cfg);

  if (uid == nullptr) {
    std::cerr << "please set valid uid ptr" << std::endl;
    print_debug("end: init_metadata_storage_module");
    return nullptr;
  }

  // decide metadata storage!
  std::string storage_module_name = ceph_cfg.get_metadata_storage_module();
  if (storage_module_name.compare(librmb::RadosMetadataStorageIma::module_name) == 0) {
    ms = new librmb::RadosMetadataStorageIma(&storage->get_io_ctx(), &cfg);
  } else {
    ms = new librmb::RadosMetadataStorageDefault(&storage->get_io_ctx());
  }

  *uid = (*opts)["namespace"] + cfg.get_user_suffix();
  std::string ns;
  if (mgr.lookup_key(*uid, &ns)) {
    storage->set_namespace(ns);
  } else {
    // use
    if (!mgr.lookup_key(*uid, &ns)) {
      std::cout << " error unable to determine namespace" << std::endl;
      delete ms;
      print_debug("end: init_metadata_storage_module");
      return nullptr;
    }
    storage->set_namespace(ns);
  }
  print_debug("end: init_metadata_storage_module");
  return ms;
}
int RmbCommands::update_attributes(librmb::RadosStorageMetadataModule *ms,
                                   std::map<std::string, std::string> *metadata) {
  std::string oid = (*opts)["set"];
  if (!oid.empty() && metadata->size() > 0) {
    for (std::map<std::string, std::string>::iterator it = metadata->begin(); it != metadata->end(); ++it) {
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
      std::cout << " saving object ..." << std::endl;
    }
  } else {
    std::cerr << " invalid number of arguments, check usage " << std::endl;
    return -1;
  }
  return 0;
}
void RmbCommands::set_output_path(librmb::CmdLineParser *parser) {
  if ((*opts).find("out") != (*opts).end()) {
    parser->set_output_dir((*opts)["out"]);
  } else {
    char outpath[PATH_MAX];
    char *home = getenv("HOME");
    if (home != NULL) {
      snprintf(outpath, sizeof(outpath), "%s/rmb", home);
    } else {
      snprintf(outpath, sizeof(outpath), "rmb");
    }
    parser->set_output_dir(outpath);
  }
}

} /* namespace librmb */
