/*
 * Token implementation for tuples.
 * See LICENSE.txt
*/

#ifndef TOKEN_H_
#define TOKEN_H_

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdlib.h> /* malloc() */
#include <string.h> /* memcpy() */

#define BLEN 8

typedef struct tokent {
	char *ptr;
	size_t len;
	char _buf[BLEN]; /* Big enough for any numeric value on my platform. */
} token;

int set_token(token *t, void *ptr, const size_t plen);

#endif
