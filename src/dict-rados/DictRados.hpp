#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

class DictRados {
public:
	librados::IoCtx io_ctx;

	std::string pool;
	std::string oid;
	std::string username;

	std::map<std::string, librados::bufferlist> readerMap;
	typename std::map<std::string, librados::bufferlist>::iterator readerMapIter;

	librados::AioCompletion *completion;
	librados::ObjectReadOperation readOperation;
	librados::bufferlist bufferList;
	std::string lookupKey;
	void *context;
	dict_lookup_callback_t *callback;

	DictRados();
	virtual ~DictRados();

	int read_config_from_uri(const char *uri);
	int parse_arguments(int argc, const char **argv);
};

#endif /* SRC_DICTRADOS_HPP_ */

