#ifndef SRC_DICTRADOS_HPP_
#define SRC_DICTRADOS_HPP_

#include <rados/librados.hpp>

class DictRados {

private:
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

	static const std::vector<std::string> explode(const std::string& str, const char& sep);

public:
	DictRados();
	virtual ~DictRados();

	int initCluster(const char *const name, const char *const clustername, uint64_t flags);
	int readConfigFromUri(const char *uri);
	int readConfigFile(const char *path);
	int parseArguments(int argc, const char **argv);
	int connect();
	void shutdown();

	int createIOContext(const char *name);
	librados::IoCtx* getIOContext() {return &io_ctx;}

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

	typename std::map<std::string, librados::bufferlist>::iterator getReaderMapIter() const {
		return readerMapIter;
	}

};

#endif /* SRC_DICTRADOS_HPP_ */
