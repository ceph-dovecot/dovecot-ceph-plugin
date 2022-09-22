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

#ifndef SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_
#define SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_

#include <string>

#include <rados/librados.hpp>

namespace librmb {
/** class RadosDictionary
 *  brief an abstract Rados Cluster
 *  details This abstract class provides the api
 *         to create a rados cluster connection:
 */
class RadosDictionary;

class RadosCluster {
 public:
  virtual ~RadosCluster() {}
  /*!
   * initialize the cluster with default user and clustername
   * @return linux error code or 0 if successful
   *
   */
  virtual int init() = 0;
  /*!
   * initialize the cluster with custom user and clustername
   * @return linux error code or 0 if successful
   */
  virtual int init(const std::string &clustername, const std::string &rados_username) = 0;
  /*! tear down cluster
   * @return linux error code or 0 if successful
   * */
  virtual void deinit() = 0;
  /*! create a storae pool
   * @param[in] pool poolname to create if not existend.
   * @return linux errror code or 0 if successful
   * */
  virtual int pool_create(const std::string &pool) = 0;
  /*! create io context
   * @param[in] pool poolname
   * @praam[in] valid io_ctx.
   * @return linux errror code or 0 if successful
   * */
  virtual int io_ctx_create(const std::string &pool, librados::IoCtx *io_ctx) = 0;
  /*!
   * read ceph configuration
   * @param[in] option option name as described in the ceph documentation
   * @param[out] value valid ptr to a string buffer.
   * @return linux error code or 0 if successful
   *
   */
  virtual int get_config_option(const char *option, std::string *value) = 0;

  /*!
     * set ceph configuration
     * @param[in] option option name as described in the ceph documentation
     * @param[out] value valid ptr to a string buffer.
     * @return linux error code or 0 if successful
     *
     */
  virtual void set_config_option(const char *option, const char *value) = 0;

  /*!
   * Check if cluster connection does exist and is working-
   * @return true if connected
   */
  virtual bool is_connected() = 0;

  /*! get placement groups for mailbox storage pool 
  */
  virtual std::vector<std::string> list_pgs_for_pool(std::string &pool_name) = 0;
  virtual std::map<std::string, std::vector<std::string>> list_pgs_osd_for_pool(std::string &pool_name) = 0;

  
};

}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_
