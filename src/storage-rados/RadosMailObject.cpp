/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "RadosMailObject.h"

using namespace librados;  // NOLINT

RadosMailObject::RadosMailObject() {
  // TODO Auto-generated constructor stub
}

RadosMailObject::~RadosMailObject() {
  // TODO Auto-generated destructor stub
}

void RadosMailObject::addXAttribute(std::string key, librados::bufferlist &bl) {
  this->writeOperation.setxattr(key.c_str(),bl);
}
void RadosMailObject::fullWrite(librados::bufferlist &bl) {
  this->writeOperation.write_full(bl);
}
