// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#include "rados-util.h"
#include <string>
#include <limits.h>
#include <iostream>
#include <sstream>
#include "encoding.h"

namespace librmb {

RadosUtils::RadosUtils() {}

RadosUtils::~RadosUtils() {}

bool RadosUtils::convert_str_to_time_t(const std::string &date, time_t *val) {
  struct tm tm = {0};
  if (strptime(date.c_str(), "%Y-%m-%d %H:%M:%S", &tm)) {
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    *val = t;
    return true;
  }
  *val = 0;
  return false;
}

bool RadosUtils::convert_string_to_date(const std::string &date_string, std::string *date) {
  time_t t;
  if (convert_str_to_time_t(date_string, &t)) {
    *date = std::to_string(t);
    return true;
  }
  return false;
}

bool RadosUtils::is_numeric(const std::string &s) {
  std::string::const_iterator it = s.begin();
  while (it != s.end() && std::isdigit(*it)) {
    ++it;
  }
  return !s.empty() && it == s.end();
}

bool RadosUtils::is_date_attribute(const rbox_metadata_key &key) {
  return (key == RBOX_METADATA_OLDV1_SAVE_TIME || key == RBOX_METADATA_RECEIVED_TIME);
}

int RadosUtils::convert_time_t_to_str(const time_t &t, std::string *ret_val) {
  char buffer[256];
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  *ret_val = std::string(buffer);
  return 0;
}

bool RadosUtils::flags_to_string(const uint8_t &flags_, std::string *flags_str) {
  std::stringstream sstream;
  sstream << std::hex << flags_;
  sstream >> *flags_str;
  return true;
}

bool RadosUtils::string_to_flags(const std::string &flags_, uint8_t *flags) {
  std::istringstream in(flags_);

  if (in >> std::hex >> *flags) {
    return true;
  }
  return false;
}

void RadosUtils::find_and_replace(std::string *source, std::string const &find, std::string const &replace) {
  for (std::string::size_type i = 0; source != nullptr && (i = source->find(find, i)) != std::string::npos;) {
    source->replace(i, find.length(), replace);
    i += replace.length();
  }
}

int RadosUtils::get_all_keys_and_values(librados::IoCtx *io_ctx, const std::string &oid,
                                        std::map<std::string, librados::bufferlist> *kv_map) {
  int err = 0;
  librados::ObjectReadOperation first_read;
  std::set<std::string> extended_keys;
#ifdef HAVE_OMAP_GET_KEYS_2
  first_read.omap_get_keys2("", LONG_MAX, &extended_keys, nullptr, &err);
#else
  first_read.omap_get_keys("", LONG_MAX, &extended_keys, &err);
#endif
  int ret = io_ctx->operate(oid.c_str(), &first_read, NULL);
  if (ret < 0) {
    return ret;
  }
  return io_ctx->omap_get_vals_by_keys(oid, extended_keys, kv_map);
}

void RadosUtils::resolve_flags(const uint8_t &flags, std::string *flat) {
  std::stringbuf buf;
  std::ostream os(&buf);

  if ((flags & 0x01) != 0) {
    os << "\\Answered ";
  }
  if ((flags & 0x02) != 0) {
    os << "\\Flagged ";
  }
  if ((flags & 0x04) != 0) {
    os << "\\Deleted ";
  }
  if ((flags & 0x08) != 0) {
    os << "\\Seen ";
  }
  if ((flags & 0x10) != 0) {
    os << "\\Draft ";
  }
  if ((flags & 0x20) != 0) {
    os << "\\Recent ";
  }
  *flat = buf.str();
}

int RadosUtils::osd_add(librados::IoCtx *ioctx, const std::string &oid, const std::string &key,
                        long long value_to_add) {
  librados::bufferlist in, out;
  encode(key, in);

  std::stringstream stream;
  stream << value_to_add;

  encode(stream.str(), in);

  return ioctx->exec(oid, "numops", "add", in, out);
}

int RadosUtils::osd_sub(librados::IoCtx *ioctx, const std::string &oid, const std::string &key,
                        long long value_to_subtract) {
  return osd_add(ioctx, oid, key, -value_to_subtract);
}

std::string RadosUtils::get_metadata(librmb::rbox_metadata_key key, std::map<std::string, ceph::bufferlist> *metadata) {
  string str_key(1, static_cast<char>(key));
  return get_metadata(str_key, metadata);
}

std::string RadosUtils::get_metadata(const std::string &key, std::map<std::string, ceph::bufferlist> *metadata) {
  std::string value;
  if (metadata->find(key) != metadata->end()) {
    value = (*metadata)[key].to_str();
  }
  return value;
}
bool RadosUtils::is_numeric(std::string &text) {
  if (text.empty()) {
    return true;
  }
  return text.find_first_not_of("0123456789") == std::string::npos;
}

bool RadosUtils::validate_metadata(map<string, ceph::bufferlist>* metadata) {
  string uid = get_metadata(RBOX_METADATA_MAIL_UID, metadata);
  string recv_time_str = get_metadata(RBOX_METADATA_RECEIVED_TIME, metadata);
  string p_size = get_metadata(RBOX_METADATA_PHYSICAL_SIZE, metadata);
  string v_size = get_metadata(RBOX_METADATA_VIRTUAL_SIZE, metadata);

  string rbox_version = get_metadata(RBOX_METADATA_VERSION, metadata);
  string mailbox_guid = get_metadata(RBOX_METADATA_MAILBOX_GUID, metadata);
  string mail_guid = get_metadata(RBOX_METADATA_GUID, metadata);
  string mb_orig_name = get_metadata(RBOX_METADATA_ORIG_MAILBOX, metadata);

  string flags = get_metadata(RBOX_METADATA_OLDV1_FLAGS, metadata);
  string pvt_flags = get_metadata(RBOX_METADATA_PVT_FLAGS, metadata);
  string from_envelope = get_metadata(RBOX_METADATA_FROM_ENVELOPE, metadata);

  int test = 0;
  test += is_numeric(uid) ? 0 : 1;
  test += is_numeric(recv_time_str) ? 0 : 1;
  test += is_numeric(p_size) ? 0 : 1;
  test += is_numeric(v_size) ? 0 : 1;
  test += is_numeric(flags) ? 0 : 1;
  test += is_numeric(pvt_flags) ? 0 : 1;

  test += rbox_version.empty() ? 1 : 0;
  test += mailbox_guid.empty() ? 1 : 0;
  test += mail_guid.empty() ? 1 : 0;
  return test == 0;


}
// assumes that destination is open and initialized with uses namespace
int RadosUtils::move_to_alt(std::string &oid, RadosStorage *primary, RadosStorage *alt_storage,
                            RadosMetadataStorage *metadata, bool inverse) {
  int ret = 0;
  ret = copy_to_alt(oid, oid, primary, alt_storage, metadata, inverse);
  if (ret >= 0) {
    if (inverse) {
      ret = alt_storage->get_io_ctx().remove(oid);
    } else {
      ret = primary->get_io_ctx().remove(oid);
    }
  }
  return ret;
}
int RadosUtils::copy_to_alt(std::string &src_oid, std::string &dest_oid, RadosStorage *primary,
                            RadosStorage *alt_storage, RadosMetadataStorage *metadata, bool inverse) {
  int ret = 0;

  // TODO; check that storage is connected and open.
  if (primary == nullptr) {
    return false;
  }
  if (alt_storage == nullptr) {
    return false;
  }

  RadosMailObject mail;
  mail.set_oid(src_oid);
  librados::ObjectWriteOperation *write_op = new librados::ObjectWriteOperation();

  if (inverse) {
    ret = alt_storage->read_mail(src_oid, mail.get_mail_buffer());
    metadata->get_storage()->set_io_ctx(&alt_storage->get_io_ctx());

  } else {
    ret = primary->read_mail(src_oid, mail.get_mail_buffer());
  }

  if (ret < 0) {
    metadata->get_storage()->set_io_ctx(&primary->get_io_ctx());
    return ret;
  }
  mail.set_mail_size(mail.get_mail_buffer()->length());

  // load the metadata;
  ret = metadata->get_storage()->load_metadata(&mail);
  if (ret < 0) {
    return ret;
  }

  mail.set_oid(dest_oid);
  metadata->get_storage()->save_metadata(write_op, &mail);

  bool async = true;
  if (inverse) {
    ret = primary->save_mail(write_op, &mail, async);
  } else {
    ret = alt_storage->save_mail(write_op, &mail, async);
  }

  bool success = false;
  std::vector<librmb::RadosMailObject *> objects;
  objects.push_back(&mail);
  if (inverse) {
    success = primary->wait_for_rados_operations(objects);
    // ret = primary->aio_operate(&primary->get_io_ctx(), dest_oid, completion, &write_op);
  } else {
    success = alt_storage->wait_for_rados_operations(objects);
    // ret = alt_storage->aio_operate(&alt_storage->get_io_ctx(), dest_oid, completion, &write_op);
  }

  return success ? 0 : 1;
}

}  // namespace librmb
