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

	std::map<std::string, librados::bufferlist> readerMap;
	typename std::map<std::string, librados::bufferlist>::iterator readerMapIter;

	librados::ObjectReadOperation readOperation;
	librados::bufferlist bufferList;
	std::string lookupKey;
	void *context;
	dict_lookup_callback_t *callback;

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

private:
	std::string oid;
	std::string full_oid;
	std::string username;

};

#endif /* SRC_DICTRADOS_HPP_ */

