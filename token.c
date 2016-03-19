#include "token.h"

/*
 * Token implementation for tuples.
 * See LICENSE.txt
*/

void init_token(token *t)
{
	memset(t, 0, sizeof(struct tokent));

	/* Optimistically assume the buffer is big enough. */
	t->len = BLEN;
	t->ptr = t->_buf;

	return 0;
}

int set_token(token *t, void *ptr, const size_t plen)
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

	memcpy(t->ptr, ptr, plen);
	t->len = plen;

	/* Postconditions */
	assert(t->ptr != NULL);
	return 0;
}
