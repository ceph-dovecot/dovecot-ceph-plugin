/*
 * RadosBox.cpp
 *
 *  Created on: May 9, 2017
 *      Author: peter
 */

extern "C" {

#include "lib.h"
#include "array.h"
#include "fs-api.h"
#include "istream.h"
//#include "str.h"
#include "dict-transaction-memory.h"
#include "dict-private.h"
#include "ostream.h"
#include "connection.h"
#include "module-dir.h"
#include "RadosBox.h"

}

#include <stdio.h>  /* defines FILENAME_MAX */
#include <unistd.h>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <limits.h>

#include <rados/librados.hpp>
#include "RadosBox.hpp"

using namespace librados;
using namespace std;

static Rados cluster;
static int cluster_ref_count;

RadosBox::RadosBox() : pool("rbd") {
}

RadosBox::~RadosBox() {
}

int RadosBox::init(const string uri, const string &username, string &error_r) {
	const char * const *args;
	int ret = 0;
	int err = 0;

	if (cluster_ref_count == 0) {
		if (ret >= 0) {
			err = cluster.init(nullptr);
			i_debug("RadosBox::init()=%d", err);
			if (err < 0) {
				error_r = "Couldn't create the cluster handle! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_parse_env(nullptr);
			i_debug("conf_parse_env()=%d", err);
			if (err < 0) {
				error_r = "Cannot parse config environment! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {

			char cCurrentPath[FILENAME_MAX];
			if (getcwd(cCurrentPath, sizeof(cCurrentPath))) {
				i_debug("Current path = %s", cCurrentPath);
			}

			//err = cluster.conf_read_file("/home/peter/.ceph/config/ceph.conf");
			err = cluster.conf_read_file(nullptr);
			i_debug("conf_read_file()=%d", err);
			if (err < 0) {
				error_r = "Cannot read config file! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.connect();
			i_debug("connect()=%d", err);
			if (err < 0) {
				error_r = "Cannot connect to cluster! " + string(strerror(-err));
				ret = -1;
			} else {
				cluster_ref_count++;
			}
		}
	}

	if (ret >= 0) {
		err = cluster.ioctx_create(pool.c_str(), io_ctx);
		i_debug("ioctx_create(pool=%s)=%d", pool.c_str(), err);
		if (err < 0) {
			error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
			cluster.shutdown();
			cluster_ref_count--;
			ret = -1;
		}
	}

	if (ret < 0) {
		i_debug("RadosBox::init(uri=%s)=%d/%s, cluster_ref_count=%d", uri.c_str(), -1, error_r.c_str(), cluster_ref_count);
		return -1;
	}

	set_username(username);
	io_ctx.set_namespace(username);

	i_debug("RadosBox::init(uri=%s)=%d, cluster_ref_count=%d", uri.c_str(), 0, cluster_ref_count);
	return 0;
}

void RadosBox::deinit() {
	i_debug("DictRados::deinit(), cluster_ref_count=%d", cluster_ref_count);

	io_ctx.close();

	if (cluster_ref_count > 0) {
		cluster_ref_count--;
		if (cluster_ref_count == 0) {
			i_debug("Shutdown cluster. Ref count = %d", cluster_ref_count);
			cluster.shutdown();
		}
	}
}

void RadosBox::set_username(const std::string& username) {
/*
	if (username.find( DICT_USERNAME_SEPARATOR) == string::npos) {
		this->username = username;
	} else {
		 escape the username
		this->username = dict_escape_string(username.c_str());
	}
*/
	this->username = username;
}



IoCtx& RadosBox::get_io_ctx(const std::string& key) {
	return io_ctx;
}

// C-API

struct rados_rbox {
	RadosBox *d;
};

static rados_rbox *test_rbox;

extern "C" {

CRadosBox * new_radosbox_inst(void) {
  i_debug("new_radosbox_inst");
  RadosBox *inst = new RadosBox();
  return (CRadosBox *) inst;
}

void delete_radosbox_inst(CRadosBox *inst) {
  i_debug("delete_radosbox_inst");
  RadosBox *box = (RadosBox *) inst;
  delete box;
}

int radosbox_init(CRadosBox *box, const char *uri) {
  i_debug("radosbox_init(uri=%s), cluster_ref_count=%d", uri, cluster_ref_count);

  RadosBox *rbox = (RadosBox *) box;

  string error_msg;
  int ret = rbox->init(uri, "t", error_msg);

  if (ret < 0) {
    delete rbox;
    //*error_r = t_strdup_printf("%s", error_msg.c_str());
    return -1;
  }

  return 0;
}

void radosbox_deinit(CRadosBox *box) {
  i_debug("radosbox_deinit(), cluster_ref_count=%d", cluster_ref_count);

  if (cluster_ref_count > 0) {
    RadosBox *d = (RadosBox *) box;
    d->deinit();
  }
}

int rbox_rados_init(const char *uri, const char **error_r) {
  const char * const *args;

  i_debug("rbox_rados_init(uri=%s), cluster_ref_count=%d", uri, cluster_ref_count);

  test_rbox = i_new(struct rados_rbox, 1);
  test_rbox->d = new RadosBox();
  RadosBox *d = test_rbox->d;

  string error_msg;
  int ret = d->init("", "t", error_msg);

  if (ret < 0) {
    delete test_rbox->d;
    test_rbox->d = nullptr;
    i_free(test_rbox);
    test_rbox = NULL;
    *error_r = t_strdup_printf("%s", error_msg.c_str());
    return -1;
  }

  return 0;
}

extern void rbox_rados_deinit() {
  i_debug("rbox_rados_deinit(), cluster_ref_count=%d", cluster_ref_count);

  if (cluster_ref_count > 0) {
    RadosBox *d = test_rbox->d;

    d->deinit();
    delete test_rbox->d;
    test_rbox->d = nullptr;

    i_free(test_rbox);
  }
}

} // extern "C"
