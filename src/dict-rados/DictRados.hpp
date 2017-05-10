#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

typedef std::shared_ptr<librados::AioCompletion> AioCompletionPtr;

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

	librados::IoCtx& get_io_ctx() {
		return io_ctx;
	}

	void set_io_ctx(const librados::IoCtx& privateCtx) {
		io_ctx = privateCtx;
	}

	std::list<AioCompletionPtr> completions;

private:
	std::string pool;
	std::string oid;
	std::string username;

	librados::IoCtx io_ctx;

	int read_config_from_uri(const std::string &uri);

};

#endif /* SRC_DICTRADOS_HPP_ */

