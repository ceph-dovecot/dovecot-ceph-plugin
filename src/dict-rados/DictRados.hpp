#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

#include <rados/librados.hpp>

class DictRados {

private:
	librados::Rados cluster;
	librados::IoCtx io_ctx;

	std::string sPool;
	std::string sOid;
	std::string sUsername;

	std::map<std::string, librados::bufferlist> readerMap;
	typename std::map<std::string, librados::bufferlist>::iterator readerMapIter;

	static const std::vector<std::string> explode(const std::string& str, const char& sep);

public:
	DictRados();
	virtual ~DictRados();

	int readConfigFromUri(const char *uri);

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
	int ioContextAioReadOperate(const std::string& oid, librados::AioCompletion* aioCompletion, librados::ObjectReadOperation *op,
			int flags, librados::bufferlist *pbl);
	int ioContextAioReadOperate(librados::AioCompletion* aioCompletion, librados::ObjectReadOperation *op, int flags,
			librados::bufferlist *pbl);
	int ioContextWriteOperate(const std::string& oid, librados::ObjectWriteOperation *op);
	int ioContextWriteOperate(librados::ObjectWriteOperation *op);
	int ioContextAioWriteOperate(const std::string& oid, librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation *op,
			int flags);
	int ioContextAioWriteOperate(librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation *op, int flags);

	void clearReaderMap();
	void incrementReaderMapIterator();
	void beginReaderMapIterator();
	bool isEndReaderMapIterator();

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

	const std::map<std::string, librados::bufferlist>& getReaderMap() const {
		return readerMap;
	}

	/*
	 std::map<std::string, librados::bufferlist>& getReaderMap() {
	 return readerMap;
	 }

	 */
	void setReaderMap(const std::map<std::string, librados::bufferlist>& readerMap) {
		this->readerMap = readerMap;
	}

	typename std::map<std::string, librados::bufferlist>::iterator getReaderMapIter() const {
		return readerMapIter;
	}

	const librados::Rados& getCluster() const {
		return cluster;
	}

	const librados::IoCtx& getIoCtx() const {
		return io_ctx;
	}
};

#endif /* SRC_DICTRADOS_HPP_ */
