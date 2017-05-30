/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_CLUSTER_H_
#define SRC_LIBRMB_RADOS_CLUSTER_H_

#include <string>

#include <rados/librados.hpp>

namespace tallence {
namespace librmb {

class RadosDictionary;
class RadosStorage;

class RadosCluster {
 public:
  RadosCluster();
  virtual ~RadosCluster();

  int init(std::string *error_r);
  void deinit();

  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);
  int storage_create(const std::string &pool, const std::string &username, const std::string &oid,
                     RadosStorage **storage);

 private:
  static librados::Rados cluster;
  static int cluster_ref_count;
};
}  // namespace librmb
}  // namespace tallence

#endif  // SRC_LIBRMB_RADOS_CLUSTER_H_
