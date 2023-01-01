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

#ifdef HAVE_CONFIG_H
#include "dovecot-ceph-plugin-config.h"
#endif

#include "rados-util.h"
#include <limits.h>
#include <string>
#include <list>
#include <iostream>
#include <sstream>
#include <set>
#include <cctype>
#include <algorithm>
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

  bool RadosUtils::is_numeric(const char *s) {
    if (s == NULL) {
      return false;
    }
    bool is_numeric = true;
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
      if (!std::isdigit(s[i])) {
        is_numeric = false;
        break;
      }
    }

    return is_numeric;
  }

  bool RadosUtils::is_date_attribute(const rbox_metadata_key &key) {
    return (key == RBOX_METADATA_OLDV1_SAVE_TIME || key == RBOX_METADATA_RECEIVED_TIME);
  }

  int RadosUtils::convert_time_t_to_str(const time_t &t, std::string *ret_val) {
    char buffer[256];
    if (t == -1) {
      *ret_val = "invalid date";
      return -1;
    }
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
  #ifdef DOVECOT_CEPH_PLUGIN_HAVE_OMAP_GET_KEYS_2
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

  /*!
    * @return reference to all write operations related with this object
    */

  void RadosUtils::get_metadata(const std::string &key, std::map<std::string, ceph::bufferlist> *metadata, char **value) {
    if (metadata->find(key) != metadata->end()) {
      *value = (*metadata)[key].c_str();
      return;
    }
    *value = NULL;
  }
  void RadosUtils::get_metadata(rbox_metadata_key key, std::map<std::string, ceph::bufferlist> *metadata, char **value) {
    string str_key(librmb::rbox_metadata_key_to_char(key));
    get_metadata(str_key, metadata, value);
  }
  bool RadosUtils::is_numeric_optional(const char *text) {
    if (text == NULL) {
      return true;  // optional
    }
    return is_numeric(text);
  }

  bool RadosUtils::validate_metadata(map<string, ceph::bufferlist> *metadata) {
    char *uid = NULL;
    get_metadata(RBOX_METADATA_MAIL_UID, metadata, &uid);
    char *recv_time_str = NULL;
    get_metadata(RBOX_METADATA_RECEIVED_TIME, metadata, &recv_time_str);
    char *p_size = NULL;
    get_metadata(RBOX_METADATA_PHYSICAL_SIZE, metadata, &p_size);
    char *v_size = NULL;
    get_metadata(RBOX_METADATA_VIRTUAL_SIZE, metadata, &v_size);

    char *rbox_version;
    get_metadata(RBOX_METADATA_VERSION, metadata, &rbox_version);
    char *mailbox_guid = NULL;
    get_metadata(RBOX_METADATA_MAILBOX_GUID, metadata, &mailbox_guid);
    char *mail_guid = NULL;
    get_metadata(RBOX_METADATA_GUID, metadata, &mail_guid);
    char *mb_orig_name = NULL;
    get_metadata(RBOX_METADATA_ORIG_MAILBOX, metadata, &mb_orig_name);

    char *flags = NULL;
    get_metadata(RBOX_METADATA_OLDV1_FLAGS, metadata, &flags);
    char *pvt_flags = NULL;
    get_metadata(RBOX_METADATA_PVT_FLAGS, metadata, &pvt_flags);
    char *from_envelope = NULL;
    get_metadata(RBOX_METADATA_FROM_ENVELOPE, metadata, &from_envelope);

    int test = 0;
    test += is_numeric(uid) ? 0 : 1;
    test += is_numeric(recv_time_str) ? 0 : 1;
    test += is_numeric(p_size) ? 0 : 1;
    test += is_numeric(v_size) ? 0 : 1;

    test += is_numeric_optional(flags) ? 0 : 1;
    test += is_numeric_optional(pvt_flags) ? 0 : 1;

    test += mailbox_guid == NULL ? 1 : 0;
    test += mail_guid == NULL ? 1 : 0;
    return test == 0;
  }
  // assumes that destination is open and initialized with uses namespace
  int RadosUtils::move_to_alt(std::string &oid, RadosStorage *primary, RadosStorage *alt_storage,
                              RadosMetadataStorage *metadata, bool inverse) {
    int ret = -1;
    ret = copy_to_alt(oid, oid, primary, alt_storage, metadata, inverse);
    if (ret > 0) {
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

    // TODO(jrse) check that storage is connected and open.
    if (primary == nullptr || alt_storage == nullptr) {
      return 0;
    }

    RadosMail mail;
    mail.set_oid(src_oid);

    librados::bufferlist *bl = new librados::bufferlist();
    mail.set_mail_buffer(bl);

    if (inverse) {
      ret = alt_storage->read_mail(src_oid)->get_ret_read_op();
      metadata->get_storage()->set_io_ctx(&alt_storage->get_io_ctx());
    } else {
      ret = primary->read_mail(src_oid)->get_ret_read_op();
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

    librados::ObjectWriteOperation write_op;  // = new librados::ObjectWriteOperation();
    metadata->get_storage()->save_metadata(&write_op, &mail);

    bool success;
    if (inverse) {
      success = primary->save_mail(&write_op, &mail);
    } else {
      success = alt_storage->save_mail(&write_op, &mail);
    }

    if (!success) {
      return 0;
    }

    return success ? 0 : 1;
  }

  static std::vector<std::string> RadosUtils::extractPgs(const std::string& str)
  {
      std::vector<std::string> tokens;

      std::stringstream ss(str);
      std::string token;
      while (std::getline(ss, token, '\n')) {
          std::string pgs = token.substr(0, 10); //take first 10 chars for pgids
          // trim the result.
          pgs.erase(std::remove_if(pgs.begin(), pgs.end(), ::isspace), pgs.end()); 
          tokens.push_back(pgs);
      }
      //skip first line (header)
      tokens.erase(tokens.begin());
      //remove last element (footer)
      tokens.pop_back();
      
      return tokens;
  }

  static std::map<std::string, std::vector<std::string>> RadosUtils::extractPgAndPrimaryOsd(const std::string& str)
  {
      std::map<std::string,std::vector<std::string>> tokens;

      std::stringstream ss(str);
      std::string token;
      bool first_line = true;
      while (std::getline(ss, token, '\n')) {
          if(first_line){
            first_line = false;
            continue;
          }
          std::vector<std::string> line = split(token,' ');
          if(line.size() < 14){
            continue;
          }
          std::string tmp_primary_osd = split(line[13],',')[0];
          std::string primary_osd = tmp_primary_osd.erase(0,1);        
          std::string pgs = line[0];
          
          auto it = tokens.find(primary_osd);
          if(it!=tokens.end()){
            tokens[primary_osd].push_back(pgs);
          }else{
            std::vector<std::string> t;
            t.push_back(pgs);
            tokens.insert({primary_osd,t});    
          }
          
      }
      
      return tokens;
  }

  static std::vector<std::string> RadosUtils::split(std::string str_to_split, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream stream(str_to_split);
    std::string token;

    while(getline(stream, token, delimiter)) {
      token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end()); 
      if(token.length() > 0) {
        tokens.push_back(token);
      }
  
    }

    return tokens;
  }

  static std::string RadosUtils::convert_to_ceph_index(const std::set<std::string> &list){
    std::ostringstream str;
    std::copy(list.begin(), list.end(), std::ostream_iterator<string>(str, ","));
    return str.str();
  }

  static std::string RadosUtils::convert_to_ceph_index(const std::string &str) {
    return str + ",";
  }

  static std::set<std::string>  RadosUtils::ceph_index_to_set(const std::string &str) {
    std::set<std::string> index;
    std::stringstream ss(str);

    while (ss.good()) {
        std::string substr;
        getline(ss, substr, ',');
                
        if(substr.length() != 0){
          index.insert(substr);
        }
                
    }
    return index;
  }
}  // namespace librmb
