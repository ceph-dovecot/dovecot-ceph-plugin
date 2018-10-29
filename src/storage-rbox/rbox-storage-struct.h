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

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_

/**
 * @brief rbox_index_header.
 *
 * rbox index_heder struct.
 */
struct rbox_index_header {
  /** increased every time a full mailbox rebuild is done **/
  uint32_t rebuild_count;
  /** unique mailbox ident **/
  guid_128_t mailbox_guid;
  /** enum dbox_index_header_flags **/
  uint8_t flags;
  uint8_t unused[3];
};

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_
