/*
 * Append only persistent dynamic array implmentation.
 * Intended as a backend for a time series database, API cloned from sdbm.
 * Author: Nicholas Hancock
 * See LICENSE.txt
*/

#ifndef TSS_H_
#define TSS_H_

#include "tss_xmacro.h"

typedef struct {
	char *dpath;
} TSS;

typedef struct {
	char *fname;
	uint8_t *data;
} token;

TSS_ERROR tss_open(TSS**, char *);
TSS_ERROR tss_close(TSS *);
#endif
