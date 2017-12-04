// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include "rados-dovecot-ceph-cfg-impl.h"

namespace librmb {

RadosDovecotCephCfgImpl::RadosDovecotCephCfgImpl(RadosStorage *storage) {
  dovecot_cfg = new RadosConfig();
  rados_cfg = new RadosCephConfig(storage);
  delete_cfg = true;
}

RadosDovecotCephCfgImpl::RadosDovecotCephCfgImpl(RadosConfig *dovecot_cfg_, RadosCephConfig *rados_cfg_) {
  dovecot_cfg = dovecot_cfg_;
  rados_cfg = rados_cfg_;
  // do not delete injected pointer
  delete_cfg = false;
}

RadosDovecotCephCfgImpl::~RadosDovecotCephCfgImpl() {
  if (delete_cfg) {
    delete dovecot_cfg;
    delete rados_cfg;
  }
}

int RadosDovecotCephCfgImpl::save_default_rados_config() {
  bool valid = rados_cfg->save_cfg() == 0 ? true : false;
  rados_cfg->set_config_valid(valid);
  return valid ? 0 : -1;
}

} /* namespace librmb */
