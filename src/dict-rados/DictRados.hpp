#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

class DictRados {
public:
	librados::IoCtx private_ctx;
	librados::IoCtx shared_ctx;

	std::string pool;

	DictRados();
	virtual ~DictRados();

	int read_config_from_uri(const char *uri);
	int parse_arguments(int argc, const char **argv);

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

private:
	std::string oid;
	std::string full_oid;
	std::string username;

};

#endif /* SRC_DICTRADOS_HPP_ */

