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
#include "ls_cmd_parser.h"
#include "mailbox_tools.h"
#include "rados-metadata-storage-module.h"

namespace librmb {

class RmbCommands {
 public:
  RmbCommands(librmb::RadosStorage *storage_, librmb::RadosCluster *cluster_,
              std::map<std::string, std::string> *opts_);
  virtual ~RmbCommands();

  static int delete_with_save_log(const std::string &save_log, const std::string &rados_cluster,
                                  const std::string &rados_user);
  void print_debug(const std::string &msg);
  static int lspools();
  int delete_mail(bool confirmed);

  int rename_user(librmb::RadosCephConfig *cfg, bool confirmed, const std::string &uid);

  int configuration(bool confirmed, librmb::RadosCephConfig &ceph_cfg);

  int load_objects(librmb::RadosStorageMetadataModule *ms, std::vector<librmb::RadosMailObject *> &mail_objects,
                   std::string &sort_string);
  int update_attributes(librmb::RadosStorageMetadataModule *ms, std::map<std::string, std::string> *metadata);
  int print_mail(std::map<std::string, librmb::RadosMailBox *> *mailbox, std::string &output_dir, bool download);
  int query_mail_storage(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::CmdLineParser *parser,
                         bool download);
  librmb::RadosStorageMetadataModule *init_metadata_storage_module(librmb::RadosCephConfig &ceph_cfg, std::string *uid);
  static bool sort_uid(librmb::RadosMailObject *i, librmb::RadosMailObject *j);
  static bool sort_recv_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j);
  static bool sort_phy_size(librmb::RadosMailObject *i, librmb::RadosMailObject *j);
  static bool sort_save_date(librmb::RadosMailObject *i, librmb::RadosMailObject *j);

  void set_output_path(librmb::CmdLineParser *parser);

 private:
  std::map<std::string, std::string> *opts;
  librmb::RadosStorage *storage;
  librmb::RadosCluster *cluster;
  bool is_debug;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_TOOLS_RMB_RMB_COMMANDS_H_ */
