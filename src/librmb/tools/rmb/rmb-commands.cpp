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

#include "../../rados-cluster-impl.h"
#include "../../rados-storage-impl.h"
#include "rados-util.h"

namespace librmb {

RmbCommands::RmbCommands(librmb::RadosStorage *storage_, librmb::RadosCluster *cluster_,
                         std::map<std::string, std::string> *opts_) {
  this->storage = storage_;
  this->cluster = cluster_;
  this->opts = opts_;
  is_debug = this->opts != nullptr ? ((*opts).find("debug") != (*opts).end()) : false;
}

RmbCommands::~RmbCommands() {
  // TODO Auto-generated destructor stub
}

// TODO:: currently untestable with  mocks.
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
  return ret;
}

int RmbCommands::rename_user(librmb::RadosDovecotCephCfg *cfg, bool confirmed,
                             const std::string &uid) {
  if (!cfg->is_user_mapping()) {
    std::cout << "Error: The configuration option generate_namespace needs to be active, to be able to rename a user"
              << std::endl;

    return -1;
  }
  if (!confirmed) {
    std::cout << "WARNING: renaming a user may lead to data loss! Do you really really want to do this? \n add "
                 "--yes-i-really-really-mean-it to confirm "
              << std::endl;
    return -1;
  }
  std::string src_ = uid + cfg->get_user_suffix();

  std::string dest_ = (*opts)["to_rename"] + cfg->get_user_suffix();
  if (src_.compare(dest_) == 0) {
    std::cout << "Error: you need to give a valid username not equal to -N" << std::endl;
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
    return -1;
    }
    exist = storage->stat_mail(dest_, &size, &save_time);
    if (exist >= 0) {
      std::cout << "Error: there already exists a configuration file: " << dest_ << std::endl;
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
    return ret;
}

int RmbCommands::configuration(bool create_config, const std::string &obj_, bool confirmed,
                               librmb::RadosCephConfig &ceph_cfg) {
  bool has_update = (*opts).find("update") != (*opts).end();
  bool has_ls = (*opts).find("print_cfg") != (*opts).end();
  if (has_update && has_ls) {
    std::cerr << "create and ls is not supported, use separately" << std::endl;
    return -1;
  }

  if (create_config) {
    std::cout << "Error: there already exists a configuration " << obj_ << std::endl;
    return -1;
  }

  if (has_ls) {
    std::cout << ceph_cfg.get_config()->to_string() << std::endl;
    return 0;
  }

  if (!has_update) {
    std::cerr << "create config failed, check parameter" << std::endl;
    return -1;
  }
  if (!confirmed) {
    std::cout << "WARNING:" << std::endl;
    std::cout << "Changing this setting, after e-mails have been stored, could lead to a situation in which "
                 "users can no longer access their e-mail!!!"
              << std::endl;
    std::cout << "To confirm pass --yes-i-really-really-mean-it " << std::endl;
    return -1;
  }
  std::size_t key_val_separator_idx = (*opts)["update"].find("=");
  if (key_val_separator_idx == std::string::npos) {
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
    return -1;
  }

  if (ceph_cfg.save_cfg() < 0) {
    std::cout << " saving cfg failed" << std::endl;
    return -1;
  }
  std::cout << " saving configuration successful" << std::endl;
  return 0;
}

bool RmbCommands::sort_uid(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_MAIL_UID);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_MAIL_UID), &sz);
  return i_uid < j_uid;
}

bool RmbCommands::sort_recv_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME), &sz);
  return i_uid < j_uid;
}

bool RmbCommands::sort_phy_size(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  std::string::size_type sz;  // alias of size_t
  std::string t = i->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE);
  long i_uid = std::stol(t, &sz);
  long j_uid = std::stol(j->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE), &sz);
  return i_uid < j_uid;
}

bool RmbCommands::sort_save_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j) {
  return *i->get_rados_save_date() < *j->get_rados_save_date();
}

int RmbCommands::load_objects(librmb::RadosStorageMetadataModule *ms,
                              std::vector<librmb::RadosMailObject *> &mail_objects, std::string &sort_string) {
  if (ms == nullptr || storage == nullptr) {
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
    if (ret != 0 || object_size <= 0) {
      std::cout << " object '" << oid << "' is not a valid mail object, size = 0" << std::endl;
      ++iter;
      continue;
    }
    mail->set_oid(oid);
    if (ms->load_metadata(mail) < 0) {
      std::cout << " loading metadata of object '" << oid << "' faild " << std::endl;
      ++iter;
      continue;
    }

    if (mail->get_metadata()->size() == 0) {
      std::cout << " pool object " << oid << " is not a mail object" << std::endl;
      ++iter;
      continue;
    }

    if (!librmb::RadosUtils::validate_metadata(mail->get_metadata())) {
      std::cout << "object : " << oid << " metadata is not valid " << std::endl;
      ++iter;
      continue;
    }

    mail->set_mail_size(object_size);
    mail->set_rados_save_date(save_date_rados);
    ++iter;
    mail_objects.push_back(mail);
  }
  std::cout << "hallo welrt" << std::endl;

  if (sort_string.compare("uid") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_uid);
  } else if (sort_string.compare("recv_date") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_recv_date);
  } else if (sort_string.compare("phy_size") == 0) {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_phy_size);
  } else {
    std::sort(mail_objects.begin(), mail_objects.end(), sort_save_date);
  }

  return 0;
}

int RmbCommands::print_mail(std::map<std::string, librmb::RadosMailBox *> *mailbox, std::string &output_dir,
                            bool download) {
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
  return 0;
}

int RmbCommands::query_mail_storage(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::CmdLineParser *parser,
                                    bool download) {
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
  return ret;
  }

} /* namespace librmb */
