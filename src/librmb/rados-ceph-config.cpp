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

#include "rados-ceph-config.h"
#include <jansson.h>

namespace librmb {

RadosCephConfig::RadosCephConfig(RadosStorage *storage_) {
  storage = storage_;
}

int RadosCephConfig::save_cfg() {
  ceph::bufferlist buffer;
  bool success = config.to_json(&buffer) ? storage->save_mail(config.get_cfg_object_name(), buffer) >= 0 : false;
  return success ? 0 : -1;
}

int RadosCephConfig::load_cfg() {
  if (config.is_valid()) {
    return 0;
  }
  ceph::bufferlist buffer;
  int ret = storage->read_mail(config.get_cfg_object_name(), &buffer);
  if (ret < 0) {
    return ret;
  }

  return config.from_json(&buffer) ? 0 : -1;
}

} /* namespace librmb */
