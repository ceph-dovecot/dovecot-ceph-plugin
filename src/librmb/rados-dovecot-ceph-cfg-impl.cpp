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
}

RadosDovecotCephCfgImpl::~RadosDovecotCephCfgImpl() {
  delete dovecot_cfg;
  delete rados_cfg;
}

} /* namespace librmb */
