#include "token.h"

/*
 * Token implementation for tuples.
 * See LICENSE.txt
*/

int init_token(token *t)
{
	memset(t, 0, sizeof(struct tokent));

	/* Optimistically assume the buffer is big enough. */
	t->len = BLEN;
	t->ptr = t->_buf;

	return 0;
}

int set_token(token *t, const void *ptr, const size_t plen)
{
	/* Preconditions: */
	assert(plen > 0);

	/* If the length we need is longer than we've had, get space for it. */
	if (t->len < plen) {
		/* Reallocate the array if we've already allocated one. */
		if (t->len > BLEN) {
			t->ptr = realloc(t->ptr, plen);
		} else {
			t->ptr = malloc(plen);
		}

		if (!t->ptr) {
			return 1;
		}
	}
    
    assert(t->ptr != NULL);

	memcpy(t->ptr, ptr, plen);
	t->len = plen;

	return 0;
}
