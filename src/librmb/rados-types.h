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

#ifndef SRC_LIBRMB_RADOS_TYPES_H_
#define SRC_LIBRMB_RADOS_TYPES_H_

namespace librmb {
#define GUID_128_SIZE 16
/**
 * The available metadata keys used as rados
 * omap / xattribute
 */
enum rbox_metadata_key {
  /**
   * mailbox global unique id the mail currently is in.
   **/
  RBOX_METADATA_MAILBOX_GUID = 'M',
  /** Globally unique identifier for the message. Preserved when
     copying. **/
  RBOX_METADATA_GUID = 'G',
  /** POP3 UIDL overriding the default format **/
  RBOX_METADATA_POP3_UIDL = 'P',
  /** POP3 message ordering (for migrated mails) **/
  RBOX_METADATA_POP3_ORDER = 'O',
  /** Received UNIX timestamp in hex **/
  RBOX_METADATA_RECEIVED_TIME = 'R',
  /** Physical message size in hex. Necessary only if it differs from
     the dbox_message_header.message_size_hex, for example because the
     message is compressed. **/
  RBOX_METADATA_PHYSICAL_SIZE = 'Z',
  /** Virtual message size in hex (line feeds counted as CRLF) **/
  RBOX_METADATA_VIRTUAL_SIZE = 'V',
  /** Pointer to external message data. Format is:
     1*(<start offset> <byte count> <options> <ref>) **/
  RBOX_METADATA_EXT_REF = 'X',
  /** Mailbox name where this message was originally saved to.
     When rebuild finds a message whose mailbox is unknown, it's
     placed to this mailbox. **/
  RBOX_METADATA_ORIG_MAILBOX = 'B',
  /**
   * original mail uid.
   */
  RBOX_METADATA_MAIL_UID = 'U',
  /**
   * Metadata version used to store this
   * object.
   */
  RBOX_METADATA_VERSION = 'I',
  /**
   * Mails from envelope
   **/
  RBOX_METADATA_FROM_ENVELOPE = 'A',
  /**
   * private flags.
   */
  RBOX_METADATA_PVT_FLAGS = 'C',
  /** metadata used by old Dovecot versions **/
  RBOX_METADATA_OLDV1_EXPUNGED = 'E',
  /** saved as uint**/
  RBOX_METADATA_OLDV1_FLAGS = 'F',
  /** list of keywords**/
  RBOX_METADATA_OLDV1_KEYWORDS = 'K',
  /** additional save time **/
  RBOX_METADATA_OLDV1_SAVE_TIME = 'S',
  /** currently unused...**/
  RBOX_METADATA_OLDV1_SPACE = ' '
};

/*!
 *  Converts the given metadata_key to it's char value.
 *  @param[in]  type  The rbox_metadata_key instance
 *  @return cost char* (e.g  type =RBOX_METADATA_MAILBOX_GUID => M)
 */
static const char *rbox_metadata_key_to_char(rbox_metadata_key type) {
  switch (type) {
    case RBOX_METADATA_MAILBOX_GUID:
      return "M";
    case RBOX_METADATA_GUID:
      return "G";
    case RBOX_METADATA_POP3_UIDL:
      return "P";
    case RBOX_METADATA_POP3_ORDER:
      return "O";
    case RBOX_METADATA_RECEIVED_TIME:
      return "R";
    case RBOX_METADATA_PHYSICAL_SIZE:
      return "Z";
    case RBOX_METADATA_VIRTUAL_SIZE:
      return "V";
    case RBOX_METADATA_EXT_REF:
      return "X";
    case RBOX_METADATA_ORIG_MAILBOX:
      return "B";
    case RBOX_METADATA_MAIL_UID:
      return "U";
    case RBOX_METADATA_VERSION:
      return "I";
    case RBOX_METADATA_FROM_ENVELOPE:
      return "A";
    case RBOX_METADATA_PVT_FLAGS:
      return "C";
    case RBOX_METADATA_OLDV1_EXPUNGED:
      return "E";
    case RBOX_METADATA_OLDV1_FLAGS:
      return "F";
    case RBOX_METADATA_OLDV1_KEYWORDS:
      return "K";
    case RBOX_METADATA_OLDV1_SAVE_TIME:
      return "S";
    case RBOX_METADATA_OLDV1_SPACE:
      return " ";
    default:
      return "";
  }
}
}  // namespace
#endif /* SRC_LIBRMB_RADOS_TYPES_H_ */
