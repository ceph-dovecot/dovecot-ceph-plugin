/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RBOX_MAIL_H_
#define SRC_STORAGE_RBOX_RBOX_MAIL_H_

#include "index-mail.h"

#include "rados-mail-object.h"

enum rbox_metadata_key {
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

  /* metadata used by old Dovecot versions */
  RBOX_METADATA_OLDV1_EXPUNGED = 'E',
  RBOX_METADATA_OLDV1_FLAGS = 'F',
  RBOX_METADATA_OLDV1_KEYWORDS = 'K',
  RBOX_METADATA_OLDV1_SAVE_TIME = 'S',
  RBOX_METADATA_OLDV1_SPACE = ' '
};

struct rbox_mail {
  struct index_mail imail;

  guid_128_t index_guid;
  guid_128_t index_oid;

  librmb::RadosMailObject *mail_object;
  char *mail_buffer;
  uint32_t last_seq;  //@TODO(jrse): init with -1
};

extern int rbox_get_index_record(struct mail *_mail);
extern struct mail *rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                                    struct mailbox_header_lookup_ctx *wanted_headers);

#endif  // SRC_STORAGE_RBOX_RBOX_MAIL_H_
