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
#include <list>

#include <rados/librados.hpp>
#include "rados-cluster.h"
#include "rados-mail.h"
#include "rados-types.h"

namespace librmb {
/** class RadosStorage
 *  brief an abstract Rados Storage
 *  details This abstract class provides the api
 *         to create a rados cluster Storage.
 */
class RadosStorage {
 public:
  virtual ~RadosStorage() {}

  /*!
   * if connected, return the valid ioCtx
   */
  virtual librados::IoCtx &get_io_ctx() = 0;

  /*!
   * if connected, return the valid ioCtx for recovery index
   */
  virtual librados::IoCtx &get_recovery_io_ctx() = 0;


  /*! get the object size and object save date
   * @param[in] oid unique ident for the object
   * @param[out] psize size of the object
   * @param[out] pmtime last modified date**/
  virtual int stat_mail(const std::string &oid, uint64_t *psize, time_t *pmtime) = 0;
  /*! set the object namespace
   * @param[in] _nspace namespace */
  virtual void set_namespace(const std::string &_nspace) = 0;
  /*! get the object namespace
   *
   * @return copy of the namespace
   * */
  virtual std::string get_namespace() = 0;
  /*! get the pool name
   * @return copy of the current p

TEST_F(StorageTest, scanForPg) {
  
librmbtest::RadosClusterMock mock_test = librmbtest::RadosClusterMock();
mock.
librmb::RadosStorageImpl underTest = librmbtest::RadosStorageImpl(mock_test);

underTest.ceph_index_append()
underTest.ceph_index_add("dkfkjdf")

}
 ool name
   * */
  virtual std::string get_pool_name() = 0;

  /* set the wait method for async operations */
  virtual void set_ceph_wait_method(enum rbox_ceph_aio_wait_method wait_method) = 0;

  /*! get the max operation size in mb
   * @return the maximal number of mb to write in a single write operation*/
  virtual int get_max_write_size() = 0;
  /*! get the max operation size in bytes
   * @return max number of bytes to write in a single write operation*/
  virtual int get_max_write_size_bytes() = 0;

  /*! get the max ceph object size 
   */
  virtual int get_max_object_size() = 0;

  /*! In case the current object size exceeds the max_write (bytes), object should be split into
   * max smaller operations and executed separately.
   *
   * @param[in] current_object ptr to a valid mailobject.
   * @param[in] write_op_xattr pointer to a write operation / if null, function will create new one.
   * @param[in] max_write max number of bytes to write in one operation
   *
   * @return <0 in case of failure
   * */
  virtual int split_buffer_and_exec_op(RadosMail *current_object, librados::ObjectWriteOperation *write_op_xattr,
                                       const uint64_t &max_write) = 0;

  /*! deletes a mail object from rados
   * @param[in] mail pointer to valid mail object
   *
   * @return <0 in case of failure. */
  virtual int delete_mail(RadosMail *mail) = 0;
  /*! delete object with given oid
   * @param[in] object identifier.
   *
   * @return <0 in case of failure
   */
  virtual int delete_mail(const std::string &oid) = 0;
  /*! asynchron execution of a write operation
   *
   * @param[in] io_ctx valid io context
   * @param[in] oid object identifier
   * @param[in] c valid pointer to a completion.
   * @param[in] op the prepared write operation
   * */
  virtual int aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                          librados::ObjectWriteOperation *op) = 0;
  /*! search for mails based on given Filter
   * @param[in] attr a list of filter attributes
   *
   * @return object iterator or librados::NObjectIterator::__EndObjectIterator */
  virtual librados::NObjectIterator find_mails(const RadosMetadata *attr) = 0;


  virtual std::set<std::string> find_mails_async(const RadosMetadata *attr, 
                                                 std::string &pool_name, 
                                                 int num_threads,
                                                 void (*ptr)(std::string&)) = 0;


  /*! open the rados connections with default cluster and username
   * @param[in] poolname the poolname to connect to, in case this one does not exists, it will be created.
   * */
  virtual int open_connection(const std::string &poolname) = 0;
  /*! open the rados connection with given user and clustername
   *
   * @param[in] poolname the poolname to connect to, in case this one does not exists, it will be created.
   * @param[in] clustername custom clustername
   * @param[in] rados_username custom username (client.xxx)
   *
   * @return linux error code or 0 if successful.
   * */
  virtual int open_connection(const std::string &poolname, const std::string &clustername,
                              const std::string &rados_username) = 0;

  /*! open the rados connections with default cluster and username
   * @param[in] poolname the poolname to connect to, in case this one does not exists, it will be created.
   * @param[in] index_pool the poolname to store recovery index objects to.
   * */
  virtual int open_connection(const std::string &poolname, const std::string &index_pool) = 0;

 /*! open the rados connection with given user and clustername
   *
   * @param[in] poolname the poolname to connect to, in case this one does not exists, it will be created.
   * @param[in] index_pool the poolname to store recovery index objects to.
   * @param[in] clustername custom clustername
   * @param[in] rados_username custom username (client.xxx)
   *
   * @return linux error code or 0 if successful.
   * */
  virtual int open_connection(const std::string &poolname, const std::string &index_pool,
                      const std::string &clustername,
                      const std::string &rados_username) = 0;
  /*!
   * close the connection. (clean up structures to allow reconnect)
   */
  virtual void close_connection() = 0;

  /*! wait for all write operations to complete
   * @param[in] completion_op_map map of write operations with matching completion objects.
   * @return false if successful !!!!
   *  */
  virtual bool wait_for_write_operations_complete(librados::AioCompletion *completion,
                                                  librados::ObjectWriteOperation *write_operation) = 0;
  /*!
   * wait for all rados operations
   *
   * @param[in] object_list list of outstanding rados objects
   *
   * @return true if successful
   */
  virtual bool wait_for_rados_operations(const std::list<librmb::RadosMail *> &object_list) = 0;

  /*! save the mail object
   *
   * @param[in] oid unique object identifier
   * @param[in] buffer the objects data
   *
   * @return linux errorcode or 0 if successful
   * */
  virtual int save_mail(const std::string &oid, librados::bufferlist &buffer) = 0;

  /**
   * append oid to index object
  */
  virtual int ceph_index_append(const std::string &oid) = 0;

  /**
   * append oids to index object
  */
  virtual int ceph_index_append(const std::set<std::string> &oids) = 0;

  /**
   * overwrite ceph index object
  */
  virtual int ceph_index_overwrite(const std::set<std::string> &oids) = 0;

  /**
   * get the ceph index object as list of oids
   * 32
  */
  virtual std::set<std::string> ceph_index_read() = 0;


  /**
   * remove oids from index object
  */
  virtual int ceph_index_delete() = 0;

  /**
   * returns the ceph index size
  */
  virtual uint64_t ceph_index_size() = 0;

  /*! read the complete mail object into bufferlist
   *
   * @param[in] oid unique object identifier
   * @param[out] buffer valid ptr to bufferlist.
   * @return linux errorcode or 0 if successful
   * */
  virtual int read_mail(const std::string &oid, librados::bufferlist *buffer) = 0;
  /*! move a object from the given namespace to the other, updates the metadata given in to_update list
   *
   * @param[in] src_oid unique identifier of source object
   * @param[in] src_ns  namespace of source object
   * @param[in] dest_oid unique identifier of destination object
   * @param[in] dest_ns namespace of destination object
   * @param[in] to_update in case metadata should be updated.
   * @param[in] delete_source in case you really want to delete the source after copy.
   * @return linux errorcode or 0 if successful
   * */
  virtual int move(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                   std::list<RadosMetadata> &to_update, bool delete_source) = 0;

  /*! copy a object from the given namespace to the other, updates the metadata given in to_update list
   * @param[in] src_oid unique identifier of source object
   * @param[in] src_ns  namespace of source object
   * @param[in] dest_oid unique identifier of destination object
   * @param[in] dest_ns namespace of destination object
   * @param[in] to_update in case metadata should be updated.
   * @return linux errorcode or 0 if successful
   */
  virtual int copy(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                   std::list<RadosMetadata> &to_update) = 0;
  /*! save the mail
   * @param[in] mail valid rados mail.   
   * @return false in case of error
   * */
  virtual bool save_mail(RadosMail *mail) = 0;
  /*!
   * save the mail
   * @param[in] write_op_xattr write operation to use
   * @param[in] mail valid mail object   
   * @return false in case of error.
   *
   */
  virtual bool save_mail(librados::ObjectWriteOperation *write_op_xattr, RadosMail *mail) = 0;
  /*! create a new RadosMail
   * create new rados Mail Object.
   *  return pointer to mail object or nullptr
   * */
  virtual librmb::RadosMail *alloc_rados_mail() = 0;
  /*! free the Rados Mail Object
   * @param[in] mail ptr to valid mail object
   * */
  virtual void free_rados_mail(librmb::RadosMail *mail) = 0;


};

}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_
