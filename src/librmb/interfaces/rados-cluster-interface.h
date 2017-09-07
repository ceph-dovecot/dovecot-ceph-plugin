/*
 * rados-cluster-Interface.h
 *
 *  Created on: Sep 4, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_CLUSTER_INTERFACE_H_
#define SRC_LIBRMB_RADOS_CLUSTER_INTERFACE_H_

namespace librmb {
class RadosDictionary;

class RadosCluster {
 public:
  virtual ~RadosCluster(){};
  virtual int init(std::string *error_r) = 0;
  virtual void deinit() = 0;

  virtual int pool_create(const std::string &pool) = 0;

  virtual int io_ctx_create(const std::string &pool) = 0;
  virtual int get_config_option(const char *option, std::string *value) = 0;

  virtual librados::IoCtx &get_io_ctx() = 0;
};

}  // namespace librmb

#endif /* SRC_LIBRMB_RADOS_CLUSTER_INTERFACE_H_ */
