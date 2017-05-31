/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
#define SRC_LIBRMB_RADOS_MAIL_OBJECT_H_

#include <string>

#include <rados/librados.hpp>

namespace librmb {

class RadosMailObject {
 public:
  RadosMailObject();
  virtual ~RadosMailObject();

  std::string getOid() { return this->oid; }
  void setOid(std::string oid) { this->oid = oid; }

  librados::ObjectWriteOperation getObjectWriteOperation() { return writeOperation; }

  void addXAttribute(std::string key, const librados::bufferlist &bl);
  void fullWrite(const librados::bufferlist &bl);

 private:
  std::string oid;
  librados::ObjectWriteOperation writeOperation;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
