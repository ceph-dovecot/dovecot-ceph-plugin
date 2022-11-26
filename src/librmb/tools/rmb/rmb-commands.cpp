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
#include <time.h>
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
RmbCommands::~RmbCommands() {}

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

  if (moved_items == nullptr) {
    return -1;
  }

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
      // TODO(jrse): worst case are alternating pool entries e.g. mail_storage ,
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

int RmbCommands::delete_namespace(librmb::RadosStorageMetadataModule *ms, std::list<librmb::RadosMail *> &mail_objects,
                                  librmb::RadosCephConfig *cfg, bool confirmed) {
  if (ms == nullptr || cfg == nullptr) {
    return -1;
  }

  librmb::CmdLineParser parser("-");
  if (parser.parse_ls_string()) {
    std::string sort_type = "uid";
    int ret = load_objects(ms, mail_objects, sort_type);
    if (ret < 0 || mail_objects.size() == 0) {
      return ret;
    }

    for (std::list<librmb::RadosMail *>::iterator it_mail = mail_objects.begin(); it_mail != mail_objects.end();
         ++it_mail) {
      (*opts)["to_delete"] = *(*it_mail)->get_oid();
      delete_mail(confirmed);
    }
    if (cfg->is_user_mapping()) {
      // delete namespace object also.
      std::cout << "user mapping active " << std::endl;
      std::string indirect_ns = (*opts)["namespace"] + cfg->get_user_suffix();
      (*opts)["to_delete"] = indirect_ns;
      storage->set_namespace("users");
      delete_mail(confirmed);
    }
  }
  return 0;
}
int RmbCommands::delete_mail(bool confirmed) {
  int ret = -1;
  print_debug("entry: delete_mail");
  if (!confirmed) {
    std::cout << "WARNING: Deleting a mail object will remove the "
                 "object from ceph, but not from dovecot index, this "
                 "may lead to corrupt mailbox\n"
              << " add --yes-i-really-really-mean-it to confirm the delete " << std::endl;
  } else {
    std::cout << " deleting mail : " << storage->get_pool_name() << " ns: " << storage->get_namespace() << std::endl;
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
  if (cfg == nullptr) {
    return -1;
  }
  if (!cfg->is_user_mapping()) {
    std::cout << "Error: To be able to rename a user, the configuration option generate_namespace needs to be active"
              << std::endl;
    print_debug("end: rename_user");
    return -1;
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

  uint64_t size = -1;
  time_t save_time = -1;
  int exist = storage->stat_mail(src_, &size, &save_time);
  if (exist < 0) {
    std::cout << "Error there does not exist a mail object with oid " << src_ << std::endl;
    print_debug("end: rename_user");
    return -1;
  }

  exist = storage->stat_mail(dest_, &size, &save_time);
  if (exist >= 0) {
    std::cout << "Error: there already exists a mail object with oid: " << dest_ << std::endl;
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
    std::cerr << "create configuration failed, check parameter" << std::endl;

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
  std::cout << "cfg: key " << key << " cfg_val: " << key_val << std::endl;

  if (ceph_cfg.save_cfg() < 0) {
    std::cout << " saving cfg failed" << std::endl;

    print_debug("end: configuration");
    return -1;
  }
  std::cout << " saving configuration successful" << std::endl;
  print_debug("end: configuration");

  return 0;
}

bool RmbCommands::sort_uid(librmb::RadosMail *i, librmb::RadosMail *j) {
  std::string::size_type sz;  // alias of size_t
  char *t;
  if (i == nullptr || j == nullptr) {
    return false;
  }
  RadosUtils::get_metadata(librmb::RBOX_METADATA_MAIL_UID, i->get_metadata(), &t);
  try {
    uint64_t i_uid = std::stol(t, &sz);
    char *m_mail_uid;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_MAIL_UID, i->get_metadata(), &m_mail_uid);
    uint64_t j_uid = std::stol(m_mail_uid, &sz);

    return i_uid < j_uid;
  } catch (std::exception &e) {
    char *uid;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_MAIL_UID, i->get_metadata(), &uid);
    std::cerr << " sort_uid: " << t << "(" << *i->get_oid() << ") or " << uid << " (" << j->get_oid()
              << ") is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_recv_date(librmb::RadosMail *i, librmb::RadosMail *j) {
  std::string::size_type sz;  // alias of size_t
  char *t;
  if (i == nullptr || j == nullptr) {
    return false;
  }
  RadosUtils::get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, i->get_metadata(), &t);
  try {
    int64_t i_uid = std::stol(t, &sz);
    char *m_time;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, i->get_metadata(), &m_time);
    int64_t j_uid = std::stol(m_time, &sz);
    return i_uid < j_uid;
  } catch (std::exception &e) {
    char *m_recv_time;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, i->get_metadata(), &m_recv_time);
    std::cerr << " sort_recv_date: " << t << " or " << m_recv_time << " is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_phy_size(librmb::RadosMail *i, librmb::RadosMail *j) {
  std::string::size_type sz;  // alias of size_t
  char *t;
  if (i == nullptr || j == nullptr) {
    return false;
  }
  RadosUtils::get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, i->get_metadata(), &t);
  try {
    uint64_t i_uid = std::stol(t, &sz);
    char *m_phy_size;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, i->get_metadata(), &m_phy_size);
    uint64_t j_uid = std::stol(m_phy_size, &sz);
    return i_uid < j_uid;
  } catch (std::exception &e) {
    char *m_phy_size;
    RadosUtils::get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, i->get_metadata(), &m_phy_size);
    std::cerr << " sort_physical_size: " << t << " or " << m_phy_size << " is not a number" << std::endl;
    return false;
  }
}

bool RmbCommands::sort_save_date(librmb::RadosMail *i, librmb::RadosMail *j) {
  if (i == nullptr || j == nullptr) {
    return false;
  }
  return i->get_rados_save_date() < j->get_rados_save_date();
}

struct AioStat {
  librmb::RadosMail *mail;
  std::list<librmb::RadosMail *> *mail_objects;
  uint64_t object_size = 0;
  time_t save_date_rados;
  librmb::RadosStorageMetadataModule *ms;
  bool load_metadata;
  librados::AioCompletion *completion;
};

static void aio_cb(rados_completion_t cb, void *arg) {
  if (arg == nullptr) {
    return;
  }
  AioStat *stat = static_cast<AioStat *>(arg);
  if (stat->completion == nullptr || stat->mail == nullptr || stat->ms == nullptr || stat->mail_objects == nullptr) {
    std::cout << "aio_cb callback failed, invalid stat object" << std::endl;
    return;
  }

  if (stat->completion->get_return_value() == 0 && stat->object_size > 0) {
    stat->mail->set_mail_size(stat->object_size);
    stat->mail->set_rados_save_date(stat->save_date_rados);
    if (stat->load_metadata) {
      if (stat->ms->load_metadata(stat->mail) < 0) {
        stat->mail->set_valid(false);
      }
      if (stat->mail->get_metadata()->empty()) {
        stat->mail->set_valid(false);
      }
      if (!librmb::RadosUtils::validate_metadata(stat->mail->get_metadata())) {
        stat->mail->set_valid(false);
      }
    }
  } else {
    stat->mail->set_valid(false);
  }
  stat->mail_objects->push_back(stat->mail);
  delete stat;
}

int RmbCommands::overwrite_ceph_object_index(std::set<std::string> &mail_oids){
    return storage->ceph_index_overwrite(mail_oids);
}
std::set<std::string> RmbCommands::load_objects(librmb::RadosStorageMetadataModule *ms){
  std::set<std::string> mail_list;
  librados::NObjectIterator iter_guid = storage->find_mails(nullptr);
  while (iter_guid != librados::NObjectIterator::__EndObjectIterator) {
      librmb::RadosMail mail;
      mail.set_oid((*iter_guid).get_oid());
     
      int load_metadata_ret = ms->load_metadata(&mail); 
      if (load_metadata_ret < 0 || !librmb::RadosUtils::validate_metadata(mail.get_metadata())) {    
         std::cerr << "metadata for object : " << mail.get_oid()->c_str() << " is not valid, skipping object " << std::endl;
         iter_guid++;     
         continue;
      }
      mail_list.insert((*iter_guid).get_oid());       
      iter_guid++;     
  } 
  return mail_list;
}
int RmbCommands::remove_ceph_object_index(){
  return storage->ceph_index_delete();
}
int RmbCommands::append_ceph_object_index(const std::set<std::string> &mail_oids){
  return storage->ceph_index_append(mail_oids);
}

int RmbCommands::load_objects(librmb::RadosStorageMetadataModule *ms, std::list<librmb::RadosMail *> &mail_objects,
                              std::string &sort_string, bool load_metadata) {
  time_t begin = time(NULL);

  print_debug("entry: load_objects");
  if (ms == nullptr || storage == nullptr) {
    print_debug("end: load_objects");
    return -1;
  }
  // TODO(jrse): Fix completions.....
  std::list<librados::AioCompletion *> completions;
  // load all objects metadata into memory
  librados::NObjectIterator iter(storage->find_mails(nullptr));
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    librmb::RadosMail *mail = new librmb::RadosMail();
    AioStat *stat = new AioStat();
    stat->mail = mail;
    stat->mail_objects = &mail_objects;
    stat->load_metadata = load_metadata;
    stat->ms = ms;
    std::string oid = iter->get_oid();
    stat->completion = librados::Rados::aio_create_completion(static_cast<void *>(stat), aio_cb, NULL);
    int ret = storage->get_io_ctx().aio_stat(oid, stat->completion, &stat->object_size, &stat->save_date_rados);
    if (ret != 0) {
      std::cout << " object '" << oid << "' is not a valid mail object, size = 0, ret code: " << ret << std::endl;
      ++iter;
      delete mail;
      delete stat;
      continue;
    }
    mail->set_oid(oid);
    completions.push_back(stat->completion);

    ++iter;
    if (is_debug) {
      std::cout << "added: mail " << *mail->get_oid() << std::endl;
    }
  }

  for (std::list<librados::AioCompletion *>::iterator it = completions.begin(); it != completions.end(); ++it) {
    (*it)->wait_for_complete_and_cb();
    (*it)->release();
  }

  if (load_metadata) {
    if (sort_string.compare("uid") == 0) {
      mail_objects.sort(sort_uid);
      // std::sort(mail_objects.begin(), mail_objects.end(), sort_uid);
    } else if (sort_string.compare("recv_date") == 0) {
      mail_objects.sort(sort_recv_date);
      // std::sort(mail_objects.begin(), mail_objects.end(), sort_recv_date);
    } else if (sort_string.compare("phy_size") == 0) {
      mail_objects.sort(sort_phy_size);
      //   std::sort(mail_objects.begin(), mail_objects.end(), sort_phy_size);
    } else {
      mail_objects.sort(sort_save_date);
      //  std::sort(mail_objects.begin(), mail_objects.end(), sort_save_date);
    }
  }
  time_t end = time(NULL);

  print_debug("end: load_objects");
  std::cout << " time elapsed loading objects: " << (end - begin) << std::endl;
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
    for (std::list<librmb::RadosMail *>::iterator it_mail = it->second->get_mails().begin();
         it_mail != it->second->get_mails().end(); ++it_mail) {
      const std::string oid = *(*it_mail)->get_oid();
      librados::bufferlist bl;
      (*it_mail)->set_mail_buffer(&bl);
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

int RmbCommands::query_mail_storage(std::list<librmb::RadosMail *> *mail_objects, librmb::CmdLineParser *parser,
                                    bool download, bool silent) {
  print_debug("entry: query_mail_storage");

  std::map<std::string, librmb::RadosMailBox *> mailbox;
  for (std::list<librmb::RadosMail *>::iterator it = mail_objects->begin(); it != mail_objects->end(); ++it) {
    std::string mailbox_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_MAILBOX_GUID));
    char *mailbox_guid = NULL;
    RadosUtils::get_metadata(mailbox_key, (*it)->get_metadata(), &mailbox_guid);
    std::string mailbox_orig_name_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_ORIG_MAILBOX));
    char *mailbox_orig_name = NULL;
    RadosUtils::get_metadata(mailbox_orig_name_key, (*it)->get_metadata(), &mailbox_orig_name);

    if (mailbox_guid == NULL || mailbox_orig_name == NULL) {
      std::cout << " mail " << *(*it)->get_oid() << " with empty mailbox guid is not valid: " << std::endl;
      continue;
    }

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
  int ret = 0;
  if (!silent) {
    std::cout << "mailbox_count: " << mailbox.size() << std::endl;
    ret = print_mail(&mailbox, parser->get_output_dir(), download);
  }
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
  RadosStorageMetadataModule *ms = nullptr;
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
  if (!(*opts)["namespace"].empty()) {
    *uid = (*opts)["namespace"] + cfg.get_user_suffix();
  }
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
        if (!librmb::RadosUtils::is_numeric(value.c_str())) {
          std::string date;
          if (librmb::RadosUtils::convert_string_to_date(value, &date)) {
            value = date;
          }
        }
      }
      librmb::RadosMail obj;
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
