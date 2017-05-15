/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <string>

#include "rados-cluster.h"
#include "rados-dictionary.h"

using std::string;

librados::Rados RadosCluster::cluster;
int RadosCluster::cluster_ref_count = 0;

RadosCluster::RadosCluster() {}

RadosCluster::~RadosCluster() {}

int RadosCluster::init(string *error_r) {
  const char *const *args;
  int ret = 0;

  if (cluster_ref_count == 0) {
    if (ret >= 0) {
      ret = cluster.init(nullptr);
      if (ret < 0) {
        *error_r = "Couldn't create the cluster handle! " + string(strerror(-ret));
      }
    }

    if (ret >= 0) {
      ret = cluster.conf_parse_env(nullptr);
      if (ret < 0) {
        *error_r = "Cannot parse config environment! " + string(strerror(-ret));
      }
    }

    if (ret >= 0) {
      ret = cluster.conf_read_file(nullptr);
      if (ret < 0) {
        *error_r = "Cannot read config file! " + string(strerror(-ret));
      }
    }

    if (ret >= 0) {
      ret = cluster.connect();
      if (ret < 0) {
        *error_r = "Cannot connect to cluster! " + string(strerror(-ret));
      } else {
        cluster_ref_count++;
      }
    }
  }

  return 0;
}

void RadosCluster::deinit() {
  if (cluster_ref_count > 0) {
    cluster_ref_count--;
    if (cluster_ref_count == 0) {
      cluster.shutdown();
    }
  }
}

int RadosCluster::dictionary_create(const string &pool, const string &username, const string &oid,
                                    RadosDictionary **dictionary) {
  if (cluster_ref_count == 0) {
    return -ENOENT;
  }

  librados::IoCtx io_ctx;

  int err = cluster.ioctx_create(pool.c_str(), io_ctx);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
    return err;
  }

  *dictionary = new RadosDictionary(&io_ctx, username, oid);
  return 0;
}
