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
#include "../../rados-cluster-impl.h"
#include "../../rados-storage-impl.h"
namespace librmb {

RmbCommands::RmbCommands(librmb::RadosStorage *storage_, librmb::RadosCluster *cluster_,
                         std::map<std::string, std::string> *opts_) {
  this->storage = storage_;
  this->cluster = cluster_;
  this->opts = opts_;
}

RmbCommands::~RmbCommands() {
  // TODO Auto-generated destructor stub
}

int RmbCommands::lspools() {
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

int RmbCommands::delete_mail(bool confirmed) {
  int ret = -1;

  if (!confirmed) {
    std::cout << "WARNING: Deleting a mail object will remove the object from ceph, but not from dovecot index, this "
                 "may lead to corrupt mailbox\n"
              << " add --yes-i-really-really-mean-it to confirm the delete " << std::endl;
  } else {
    int ret = storage->delete_mail((*opts)["to_delete"]);
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

int RmbCommands::config_option(bool create_config, const std::string &obj_, bool confirmed,
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

} /* namespace librmb */
