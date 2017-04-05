#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

class DictRados {

private:

	DictRados(const DictRados& that);
	DictRados& operator=(const DictRados& that);

	librados::Rados cluster;
	librados::IoCtx io_ctx;

	std::string sPool;
	std::string sOid;
	std::string sConfig;
	std::string sUsername;
	std::string sClusterName;
	std::string sClusterUser;

	std::map<std::string, librados::bufferlist> readerMap;
	typename std::map<std::string, librados::bufferlist>::iterator readerMapIter;

	librados::ObjectReadOperation readOperation;
	librados::bufferlist bufferList;
	std::string lookupKey;
	void *context;
	dict_lookup_callback_t *callback;

	static const std::vector<std::string> explode(const std::string& str, const char& sep);

public:
	DictRados();
	virtual ~DictRados();

	int initCluster(const char *const name, const char *const clustername, uint64_t flags);
	int initCluster(uint64_t flags);
	int readConfigFromUri(const char *uri);
	int readConfigFile(const char *path);
	int parseArguments(int argc, const char **argv);
	int connect();
	void shutdown();
	librados::AioCompletion* createCompletion();
	librados::AioCompletion* createCompletion(void *cb_arg, librados::callback_t cb_complete, librados::callback_t cb_safe);

	int createIOContext(const char *name);
	void ioContextClose();
	void ioContextSetNamspace(const std::string& nspace);
    int ioContextReadOperate(const std::string& oid, librados::ObjectReadOperation *op, librados::bufferlist *pbl);
    int ioContextReadOperate(librados::ObjectReadOperation *op, librados::bufferlist *pbl);
    int ioContextAioReadOperate(const std::string& oid, librados::AioCompletion* aioCompletion, librados::ObjectReadOperation *op, int flags, librados::bufferlist *pbl);
    int ioContextAioReadOperate(librados::AioCompletion* aioCompletion, librados::ObjectReadOperation *op, int flags, librados::bufferlist *pbl);
    int ioContextWriteOperate(const std::string& oid, librados::ObjectWriteOperation *op);
    int ioContextWriteOperate(librados::ObjectWriteOperation *op);
    int ioContextAioWriteOperate(const std::string& oid, librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation *op, int flags);
    int ioContextAioWriteOperate(librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation *op, int flags);

    void clearReaderMap();
	void incrementReaderMapIterator();
	void beginReaderMapIterator();
	bool isEndReaderMapIterator();

	const std::string& getConfig() const {
		return sConfig;
	}

	void setConfig(const std::string& config) {
		sConfig = config;
	}

	const std::string& getOid() const {
		return sOid;
	}

	void setOid(const std::string& oid) {
		sOid = oid;
	}

	const std::string& getPool() const {
		return sPool;
	}

	void setPool(const std::string& pool) {
		sPool = pool;
	}

	const std::string& getUsername() const {
		return sUsername;
	}

	void setUsername(const std::string& username) {
		sUsername = username;
	}

	const std::string& getClusterName() const {
		return sClusterName;
	}

	void setClusterName(const std::string& clusterName) {
		sClusterName = clusterName;
	}

	const std::string& getClusterUser() const {
		return sClusterUser;
	}

	void setClusterUser(const std::string& clusterUser) {
		sClusterUser = clusterUser;
	}

	const std::map<std::string, librados::bufferlist>& getReaderMap() const {
		return readerMap;
	}

	std::map<std::string, librados::bufferlist>& getReaderMap() {
		return readerMap;
	}

	void setReaderMap(
			const std::map<std::string, librados::bufferlist>& readerMap) {
		this->readerMap = readerMap;
	}

	typename std::map<std::string, librados::bufferlist>::iterator getReaderMapIter() const {
		return readerMapIter;
	}

	librados::bufferlist& getBufferList() {
		return bufferList;
	}

	librados::ObjectReadOperation& getReadOperation() {
		return readOperation;
	}

	const std::string& getLookupKey() const {
		return lookupKey;
	}

	void setLookupKey(const std::string& lookupKey) {
		this->lookupKey = lookupKey;
	}

	void* getContext() const {
		return context;
	}

	void setContext(void* context) {
		this->context = context;
	}

	dict_lookup_callback_t* getCallback() const {
		return callback;
	}

	void setCallback(dict_lookup_callback_t* callback) {
		this->callback = callback;
	}
};

#endif /* SRC_DICTRADOS_HPP_ */
