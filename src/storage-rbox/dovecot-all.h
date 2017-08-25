/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_DOVECOT_ALL_H_
#define SRC_STORAGE_RBOX_DOVECOT_ALL_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

#include "lib.h"
#include "typeof-def.h"

#include "str.h"

#include "index-storage.h"
#include "index-mail.h"
#include "index-rebuild.h"

#include "mail-storage.h"
#include "mail-storage-private.h"
#include "mail-index-modseq.h"
#include "mailbox-list-private.h"
#include "mailbox-uidvalidity.h"

#ifdef HAVE_INDEX_POP3_UIDL_H
#include "index-pop3-uidl.h"
#endif

#pragma GCC diagnostic pop

// Dovecot 2.2.21 specials
#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

#endif  // SRC_STORAGE_RBOX_DOVECOT_ALL_H_
