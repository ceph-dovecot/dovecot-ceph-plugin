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

#ifndef SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_
#define SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_

#include <string>
#include <map>

#include "rados-mail-object.h"
#include <rados/librados.hpp>
#include "rados-cluster.h"

namespace librmb {

class RadosStorage {
 public:
  virtual ~RadosStorage() {}

  virtual librados::IoCtx &get_io_ctx() = 0;
  /* get the object size and object save date  */
  virtual int stat_mail(const std::string &oid, uint64_t *psize, time_t *pmtime) = 0;
  /* set the object namespace */
  virtual void set_namespace(const std::string &_nspace) = 0;
  /* get the object namespace */
  virtual std::string get_namespace() = 0;
  /* get the pool name */
  virtual std::string get_pool_name() = 0;

  /* get the max object size in mb */
  virtual int get_max_write_size() = 0;
  /* get the max object size in bytes */
  virtual int get_max_write_size_bytes() = 0;

  /* In case the current object size exceeds the max_write (bytes), object should be split into
   * max smaller operations and executed separately. */
  virtual int split_buffer_and_exec_op(RadosMailObject *current_object, librados::ObjectWriteOperation *write_op_xattr,
                                       const uint64_t &max_write) = 0;

  /* deletes a mail object from rados*/
  virtual int delete_mail(RadosMailObject *mail) = 0;
  virtual int delete_mail(const std::string &oid) = 0;
  /* asynchron execution of a write operation */
  virtual int aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                          librados::ObjectWriteOperation *op) = 0;
  /* search for mails based on given Filter */
  virtual librados::NObjectIterator find_mails(const RadosMetadata *attr) = 0;
  /* open the rados connections with default cluster and username */
  virtual int open_connection(const std::string &poolname) = 0;
  /* open the rados connection with given user and clustername */
  virtual int open_connection(const std::string &poolname, const std::string &clustername,
                              const std::string &rados_username) = 0;
  virtual void close_connection() = 0;

  /* wait for all write operations to complete */
  virtual bool wait_for_write_operations_complete(
      std::map<librados::AioCompletion*, librados::ObjectWriteOperation*>* completion_op_map) = 0;
  virtual bool wait_for_rados_operations(const std::vector<librmb::RadosMailObject *> &object_list) = 0;

  /* save the mail object */
  virtual int save_mail(const std::string &oid, librados::bufferlist &buffer) = 0;
  /* read the complete mail object into bufferlist */
  virtual int read_mail(const std::string &oid, librados::bufferlist *buffer) = 0;
  /* move a object from the given namespace to the other, updates the metadata given in to_update list */
  virtual int move(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                   std::list<RadosMetadata> &to_update, bool delete_source) = 0;
  /* copy a object from the given namespace to the other, updates the metadata given in to_update list */
  virtual int copy(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                   std::list<RadosMetadata> &to_update) = 0;
  /* save the mail */
  virtual bool save_mail(RadosMailObject *mail, bool &save_async) = 0;
  virtual bool save_mail(librados::ObjectWriteOperation *write_op_xattr, RadosMailObject *mail, bool save_async) = 0;
  /* create a new RadosMailObject */
  virtual librmb::RadosMailObject *alloc_mail_object() = 0;
  /* free the Rados Mail Object */
  virtual void free_mail_object(librmb::RadosMailObject *mail) = 0;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_
