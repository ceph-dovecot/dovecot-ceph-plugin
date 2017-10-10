// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
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

enum rbox_metadata_key {
  /*
   * mailbox global unique id the mail currently is in.
   */
  RBOX_METADATA_MAILBOX_GUID = 'M',
  /* Globally unique identifier for the message. Preserved when
     copying. */
  RBOX_METADATA_GUID = 'G',
  /* POP3 UIDL overriding the default format */
  RBOX_METADATA_POP3_UIDL = 'P',
  /* POP3 message ordering (for migrated mails) */
  RBOX_METADATA_POP3_ORDER = 'O',
  /* Received UNIX timestamp in hex */
  RBOX_METADATA_RECEIVED_TIME = 'R',
  /* Physical message size in hex. Necessary only if it differs from
     the dbox_message_header.message_size_hex, for example because the
     message is compressed. */
  RBOX_METADATA_PHYSICAL_SIZE = 'Z',
  /* Virtual message size in hex (line feeds counted as CRLF) */
  RBOX_METADATA_VIRTUAL_SIZE = 'V',
  /* Pointer to external message data. Format is:
     1*(<start offset> <byte count> <options> <ref>) */
  RBOX_METADATA_EXT_REF = 'X',
  /* Mailbox name where this message was originally saved to.
     When rebuild finds a message whose mailbox is unknown, it's
     placed to this mailbox. */
  RBOX_METADATA_ORIG_MAILBOX = 'B',

  RBOX_METADATA_MAIL_UID = 'U',
  RBOX_METADATA_VERSION = 'I',
  /*
   * Mails from envelope
   */
  RBOX_METADATA_FROM_ENVELOPE = 'A',
  RBOX_METADATA_PVT_FLAGS = 'C',
  /* metadata used by old Dovecot versions */
  RBOX_METADATA_OLDV1_EXPUNGED = 'E',
  RBOX_METADATA_OLDV1_FLAGS = 'F',
  RBOX_METADATA_OLDV1_KEYWORDS = 'K',
  RBOX_METADATA_OLDV1_SAVE_TIME = 'S',
  RBOX_METADATA_OLDV1_SPACE = ' '
};
}  // namespace
#endif /* SRC_LIBRMB_RADOS_TYPES_H_ */
