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

#ifndef SRC_LIBRMB_RADOS_GUID_GENERATOR_H_
#define SRC_LIBRMB_RADOS_GUID_GENERATOR_H_

namespace librmb {
/**
 * Rados Guid Generator
 *
 * Abstract class provides the interface to
 * reimplement the guid generation
 *
 */
class RadosGuidGenerator {
 public:
  virtual ~RadosGuidGenerator(){};
  /*!
   * @param[out] guid global unique identifier
   */
  virtual void generate_guid(std::string *guid) = 0;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_GUID_GENERATOR_H_ */
