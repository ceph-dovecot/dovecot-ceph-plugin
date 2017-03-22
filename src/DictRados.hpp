#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

#include <rados/librados.hpp>

class DictRados {

private:
	librados::Rados cluster;
	librados::IoCtx io_ctx;

public:
	DictRados();
	virtual ~DictRados();

	int initCluster(const char *const name, const char *const clustername, uint64_t flags);
	int readConfigFile(const char *path);
	int parseArguments(int argc, const char **argv);
	int connect();
	void shutdown();

	int createIOContext(const char *name);
	librados::IoCtx* getIOContext() {return &io_ctx;}

};

#endif /* SRC_DICTRADOS_HPP_ */
