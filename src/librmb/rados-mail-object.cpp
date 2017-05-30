/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rados-mail-object.h"

using namespace librados;          // NOLINT
using namespace tallence::librmb;  // NOLINT

RadosMailObject::RadosMailObject() {
  // TODO(jrse): Auto-generated constructor stub
}

RadosMailObject::~RadosMailObject() {
  // TODO(jrse): Auto-generated destructor stub
}

void RadosMailObject::addXAttribute(std::string key, const librados::bufferlist &bl) {
  this->writeOperation.setxattr(key.c_str(), bl);
}
void RadosMailObject::fullWrite(const librados::bufferlist &bl) { this->writeOperation.write_full(bl); }
