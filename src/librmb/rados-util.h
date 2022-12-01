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

#ifndef SRC_LIBRMB_RADOS_UTIL_H_
#define SRC_LIBRMB_RADOS_UTIL_H_

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <map>
#include <regex>

#include <string>
#include <map>
#include <rados/librados.hpp>
#include "rados-storage.h"
#include "rados-metadata-storage.h"
#include "rados-types.h"

namespace librmb {

/**
 * Rados Utils
 *
 * Utility class with usefull helper functions.
 *
 */
class RadosUtils {
 public:
  RadosUtils();
  virtual ~RadosUtils();
  /*!
   * convert given date string to time_t
   *
   * @param[in] date Date format: %Y-%m-%d %H:%M:%S
   * @param[out] val ptr to time_t.
   * @return true if sucessfull.
   */
  static bool convert_str_to_time_t(const std::string &date, time_t *val);
  /*!
   * check if given string is a numeric value.
   * @param[in] s string if s is empty => false
   * @return true if given string is numeric.
   */
  static bool is_numeric(const char *s);
  /*!
   * check if given string is a numeric value.
   * @param[in] text string, if string is empty => true
   * @return true if given string is numeric.
   */
  static bool is_numeric_optional(const char *text);
  /*!
   * checks if key is a data attribute
   */
  static bool is_date_attribute(const rbox_metadata_key &key);
  /*!
   * converts given data_string to numeric string
   * @param[in] date_string Date format: %Y-%m-%d %H:%M:%S
   * @param[out] date : unix timestamp.
   */
  static bool convert_string_to_date(const std::string &date_string, std::string *date);
  /*!
   * converts given time_to to string %Y-%m-%d %H:%M:%S
   * @param[in] t time_t
   * @param[out] ret_val : ptr to valid string buffer.
   * @return <0 error
   */
  static int convert_time_t_to_str(const time_t &t, std::string *ret_val);
  /*!
   * converts flags to hex string
   * @param[in] flags flags
   * @param[out] ptr to string buffer:
   * @return false if not sucessful
   */
  static bool flags_to_string(const uint8_t &flags, std::string *flags_str);

  /*!
   * converts hex string to uint8_t
   * @param[in] flags_str flags (e.g. 0x03
   * @param[out] flags to uint8_t
   * @return false if not sucessful
   */
  static bool string_to_flags(const std::string &flags_str, uint8_t *flags);

  /*!
   * replace string in text.
   * @param[in,out] source: source string
   * @param[in] find : text to find.
   * @param[in] replace: text to replace.
   */
  static void find_and_replace(std::string *source, std::string const &find, std::string const &replace);

  /*!
   * get a list of key value pairs
   * @param[in] io_ctx valid io_ctx
   * @param[in] oid: unique identifier
   * @param[out] kv_map valid ptr to key value map.
   */
  static int get_all_keys_and_values(librados::IoCtx *io_ctx, const std::string &oid,
                                     std::map<std::string, librados::bufferlist> *kv_map);
  /*!
   * get the text representation of uint flags.
   * @param[in] flags
   * @param[out] flat : string representation
   */
  static void resolve_flags(const uint8_t &flags, std::string *flat);
  /*!
   * copy object to alternative storage
   * @param[in] src_oid
   * @param[in] dest_oid
   * @param[in] primary rados primary storage
   * @param[in] alt_storage rados alternative storage
   * @param[in] metadata storage
   * @param[in] bool inverse if true, copy from alt to primary.
   * @return linux error code or 0 if sucessful
   */
  
  /***SARA: there is no use of this method. 
   * Also it invokes save_mail from RadosStorage that does not exsit anymore*/
  // static int copy_to_alt(std::string &src_oid, std::string &dest_oid, RadosStorage *primary, RadosStorage *alt_storage,
  //                        RadosMetadataStorage *metadata, bool inverse);
  /*!
   * move object to alternative storage
   * @param[in] src_oid
   * @param[in] dest_oid
   * @param[in] primary rados primary storage
   * @param[in] alt_storage rados alternative storage
   * @param[in] metadata storage
   * @param[in] bool inverse if true, move from alt to primary.
   * @return linux error code or 0 if sucessful
   */
  static int move_to_alt(std::string &oid, RadosStorage *primary, RadosStorage *alt_storage,
                         RadosMetadataStorage *metadata, bool inverse);
  /*!
   * increment (add) value directly on osd
   * @param[in] ioctx
   * @param[in] oid
   * @param[in] key
   * @param[in] value_to_add
   *
   * @return linux error code or 0 if sucessful
   */
  static int osd_add(librados::IoCtx *ioctx, const std::string &oid, const std::string &key, long long value_to_add);
  /*!
   * decrement (sub) value directly on osd
   * @param[in] ioctx
   * @param[in] oid
   * @param[in] key
   * @param[in] value_to_subtract
   *
   * @return linux error code or 0 if sucessful
   */
  static int osd_sub(librados::IoCtx *ioctx, const std::string &oid, const std::string &key,
                     long long value_to_subtract);

  /*!
   * check all given metadata key is valid
   *
   * @param[in] metadata
   * @return true if all keys and value are correct. (type, name, value)
   */
  static bool validate_metadata(std::map<std::string, ceph::bufferlist> *metadata);
  /*!
   * get metadata
   *
   * @param[in] key
   * @param[int] valid pointer to metadata map
   * @return the metadata value
   */
  static void get_metadata(const std::string &key, std::map<std::string, ceph::bufferlist> *metadata, char **value);

  /*!
   * get metadata
   *
   * @param[in] key
   * @param[int] valid pointer to metadata map
   * @return the metadata value
   */
  static void get_metadata(rbox_metadata_key key, std::map<std::string, ceph::bufferlist> *metadata, char **value);


  /**
   * POC Implemnentation to extract pgs and primary osds from mon_command output!
   **/
  static std::vector<std::string> extractPgs(const std::string& str);

  static std::map<std::string, std::vector<std::string>> extractPgAndPrimaryOsd(const std::string& str);

  static std::vector<std::string> split(std::string str_to_split, char delimiter);
  static std::string& convert_set_to_string( const std::set<std::string> &oids );
  static std::set<std::string>& convert_string_to_set(std::string &buffer);

};



}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_UTIL_H_
