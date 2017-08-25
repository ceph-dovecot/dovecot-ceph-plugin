/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_DICT_RADOS_DOVECOT_DICT_H_
#define SRC_DICT_RADOS_DOVECOT_DICT_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

#include "lib.h"
#include "dict-private.h"

#pragma GCC diagnostic pop

// Dovecot 2.2.21 specials
#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

#endif  // SRC_DICT_RADOS_DOVECOT_DICT_H_
