/*
 * Token implementation for tuples.
 * See LICENSE.txt
*/

#ifndef TOKEN_H_
#define TOKEN_H_

#include <assert.h>	/* assert() */
#include <stddef.h>	/* size_t */
#include <stdlib.h>	/* malloc() */
#include <string.h>	/* memcpy() */
#include "lz4.h"	/* lz4 compression. */

#define BLEN 8

typedef struct tokent {
	char *ptr;
	size_t len;
	char _buf[BLEN]; /* Big enough for any numeric value on my platform. */
	LZ4_stream_t lz4s; /* Used for lz4 stream compression. */
	int _cbufIndex;
	char _cbuf[LZ4_COMPRESSBOUND(BLEN)];
	char _bufLast[BLEN];
} token;

int init_token(token *t);
int set_token(token *t, void *ptr, const size_t plen);

#endif
