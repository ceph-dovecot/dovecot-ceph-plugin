#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

class DictRados {
public:
	DictRados();
	virtual ~DictRados();

	int init(const std::string uri, const std::string &username, std::string &error_r);
	void deinit();

	const std::string get_full_oid(const std::string& key);
	const std::string get_shared_oid();
	const std::string get_private_oid();

	const std::string& get_oid() const {
		return oid;
	}

	void set_oid(const std::string& oid) {
		this->oid = oid;
	}

	const std::string& get_username() const {
		return username;
	}

	void set_username(const std::string& username);

	librados::IoCtx& get_io_ctx(const std::string& key);

	const std::string& get_pool() const {
		return pool;
	}

	void set_pool(const std::string& pool) {
		this->pool = pool;
	}

	librados::IoCtx& get_private_ctx() {
		return private_ctx;
	}

	void set_private_ctx(const librados::IoCtx& privateCtx) {
		private_ctx = privateCtx;
	}

	librados::IoCtx& get_shared_ctx() {
		return shared_ctx;
	}

	void set_shared_ctx(const librados::IoCtx& sharedCtx) {
		shared_ctx = sharedCtx;
	}

private:
	std::string pool;
	std::string oid;
	std::string username;

	librados::IoCtx private_ctx;
	librados::IoCtx shared_ctx;

	int read_config_from_uri(const std::string &uri);

};

#endif /* SRC_DICTRADOS_HPP_ */

