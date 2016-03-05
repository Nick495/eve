#include "token.h"

/*
 * Token implementation for tuples.
 * See LICENSE.txt
*/

int set_token(token *t, void *ptr, const size_t plen)
{
	/* Preconditions */
	assert(plen > 0);

	t->ptr = NULL;

	if (plen <= BLEN) {
		t->ptr = t->_buf; /* Small optimization. */
	} else {
		t->ptr = malloc(sizeof(plen));
	}

	if (!t->ptr) {
		return 1;
	}

	memcpy(t->ptr, ptr, plen);
	t->len = plen;

	/* Postconditions */
	assert(t->ptr != NULL);
	return 0;
}
