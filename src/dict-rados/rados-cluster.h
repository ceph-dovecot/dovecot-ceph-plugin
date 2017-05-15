/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_DICT_RADOS_RADOS_CLUSTER_H_
#define SRC_DICT_RADOS_RADOS_CLUSTER_H_

#include <string>

#include <rados/librados.hpp>

class RadosDictionary;

class RadosCluster {
 public:
  RadosCluster();
  virtual ~RadosCluster();

  int init(std::string *error_r);
  void deinit();

  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);

 private:
  static librados::Rados cluster;
  static int cluster_ref_count;
};

#endif /* SRC_DICT_RADOS_RADOS_CLUSTER_H_ */
