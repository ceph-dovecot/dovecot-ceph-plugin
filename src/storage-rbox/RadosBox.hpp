/*
 * RadosBox.hpp
 *
 *  Created on: May 9, 2017
 *      Author: peter
 */

#ifndef SRC_STORAGE_RBOX_RADOSBOX_HPP_
#define SRC_STORAGE_RBOX_RADOSBOX_HPP_

#include <rados/librados.hpp>

class RadosBox {
public:
	RadosBox();
	virtual ~RadosBox();

	int init(const std::string uri, const std::string &username, std::string &error_r);
	void deinit();

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

private:
	std::string pool;
	std::string oid;
	std::string username;

	librados::IoCtx io_ctx;

};

#endif /* SRC_STORAGE_RBOX_RADOSBOX_HPP_ */
