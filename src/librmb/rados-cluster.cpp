/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <list>
#include <string>
#include <utility>

#include "rados-cluster.h"
#include "rados-dictionary.h"
#include "rados-storage.h"
using std::list;
using std::pair;
using std::string;

using namespace librmb;  // NOLINT

librados::Rados RadosCluster::cluster;
int RadosCluster::cluster_ref_count = 0;
const char *RadosCluster::CFG_OSD_MAX_WRITE_SIZE = "osd_max_write_size";

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

int RadosCluster::pool_create(const string &pool) {
  // pool exists? else create
  list<pair<int64_t, string>> pool_list;
  int err = cluster.pool_list2(pool_list);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot list RADOS pools: %s", strerror(-err));
    return err;
  }

  bool pool_found = false;
  for (list<pair<int64_t, string>>::iterator it = pool_list.begin(); it != pool_list.end(); ++it) {
    if ((*it).second.compare(pool) == 0) {
      pool_found = true;
      break;
    }
  }

  if (pool_found != true) {
    err = cluster.pool_create(pool.c_str());
    if (err < 0) {
      // *error_r = t_strdup_printf("Cannot create RADOS pool %s: %s", pool.c_str(), strerror(-err));
    }
  }
  return err;
}

int RadosCluster::dictionary_create(const string &pool, const string &username, const string &oid,
                                    RadosDictionary **dictionary) {
  if (cluster_ref_count == 0) {
    return -ENOENT;
  }

  // pool exists? else create
  int err = pool_create(pool);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot list RADOS pools: %s", strerror(-err));
    return err;
  }

  librados::IoCtx io_ctx;
  err = cluster.ioctx_create(pool.c_str(), io_ctx);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
    return err;
  }

  *dictionary = new RadosDictionary(&io_ctx, username, oid);
  return 0;
}

int RadosCluster::storage_create(const string &pool, RadosStorage **storage) {
  if (cluster_ref_count == 0) {
    return -ENOENT;
  }

  // pool exists? else create
  int err = pool_create(pool);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot list RADOS pools: %s", strerror(-err));
    return err;
  }

  librados::IoCtx io_ctx;
  err = cluster.ioctx_create(pool.c_str(), io_ctx);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
    return err;
  }

  std::string max_write_size;
  err = cluster.conf_get(RadosCluster::CFG_OSD_MAX_WRITE_SIZE, max_write_size);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
    return err;
  }

  // "found: max write size " << max_write_size.c_str() << "\n";

  *storage = new RadosStorage(&io_ctx, std::stoi(max_write_size));
  return 0;
}
