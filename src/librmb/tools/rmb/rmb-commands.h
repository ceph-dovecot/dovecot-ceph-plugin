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

#ifndef SRC_LIBRMB_TOOLS_RMB_RMB_COMMANDS_H_
#define SRC_LIBRMB_TOOLS_RMB_RMB_COMMANDS_H_
#include <iostream>
#include <map>
#include <string>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <iterator>
#include <list>

#include "rados-storage.h"
#include "rados-cluster.h"
#include "rados-metadata-storage.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-ceph-config.h"
namespace librmb {

class RmbCommands {
 public:
  RmbCommands(librmb::RadosStorage *storage_, librmb::RadosCluster *cluster_,
              std::map<std::string, std::string> *opts_);
  virtual ~RmbCommands();
  static int lspools();
  int delete_mail(bool confirmed);

  int rename_user(librmb::RadosDovecotCephCfg *cfg, bool confirmed, const std::string &uid);

  int config_option(bool create_config, const std::string &obj_, bool confirmed, librmb::RadosCephConfig &ceph_cfg);

 private:
  std::map<std::string, std::string> *opts;
  librmb::RadosStorage *storage;
  librmb::RadosCluster *cluster;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_TOOLS_RMB_RMB_COMMANDS_H_ */
