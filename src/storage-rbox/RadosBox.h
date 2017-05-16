/*
 * RboxRados.h
 *
 *  Created on: May 10, 2017
 *      Author: peter
 */

#ifndef SRC_STORAGE_RBOX_RADOSBOX_H_
#define SRC_STORAGE_RBOX_RADOSBOX_H_

typedef void CRadosBox;

#ifdef __cplusplus
extern "C" {
#endif

#include "lib.h"

CRadosBox * new_radosbox_inst(void);
void delete_radosbox_inst(CRadosBox *inst);

int radosbox_init(CRadosBox *box, const char *uri);
void radosbox_deinit(CRadosBox *box);

extern int rbox_rados_init(const char *uri, const char **error_r);
extern void rbox_rados_deinit();

#ifdef __cplusplus
}
#endif

#endif /* SRC_STORAGE_RBOX_RADOSBOX_H_ */
