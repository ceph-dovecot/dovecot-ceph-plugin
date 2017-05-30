/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOSMAILOBJECT_H_
#define SRC_STORAGE_RADOS_RADOSMAILOBJECT_H_

#include <rados/librados.hpp>
#include <string>

                    class RadosMailObject {
 public:
  RadosMailObject();
  virtual ~RadosMailObject();

  std::string getOid(){return this->oid;}
  void setOid(std::string oid){this->oid = oid;}

  librados::ObjectWriteOperation getObjectWriteOperation(){return writeOperation;}

  void addXAttribute(std::string key, librados::bufferlist &bl);
  void fullWrite(librados::bufferlist &bl);

 private:
   std::string oid;
   librados::ObjectWriteOperation writeOperation;
};


#endif /* SRC_STORAGE_RADOS_RADOSMAILOBJECT_H_ */
