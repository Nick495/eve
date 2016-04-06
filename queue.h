#ifndef QUEUE_H_
#define QUEUE_H_

#include <stdlib.h>	/* write() */
#include <string.h>	/* memcpy(), memmove() */
#include <unistd.h>	/* write() */
#include <assert.h> /* assert() */

#include "lz4/lib/lz4.h"

#define PAGESIZE 16384

struct queue {
	char *page;
	void *data;
	int fd; /* use and cap are ints instead of size_t's because of lz4. */
	unsigned int dUse;
	unsigned int dCap;
	unsigned int eleSize;
	unsigned int pUse;
	unsigned int pSize;
	unsigned int pEleCount;
};

int
queue_init(struct queue *q, int fd, unsigned int size, unsigned int bufCount);

void
queue_free(struct queue *q);

int
queue_write(struct queue *q);

/* Compress existing elements in the queue. */
int
queue_compress(struct queue *q);

int
queue_push(struct queue *q, const void *data);

int
queue_commit(struct queue *q);

#endif
