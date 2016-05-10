#ifndef READLINE_H_
#define READLINE_H_

#include <stdlib.h>	/* malloc(), free() */
#include <assert.h>	/* assert() */
#include <sys/types.h>	/* ssize_t */
#include <unistd.h>	/* read() */

struct rl_data {
	char *buf;	/* base pointer of buf. */
	char *bptr;	/* pointer to head of buf. */
	ssize_t blen;	/* number of valid bytes in buf. */
	size_t bcap;	/* capacity of buf. */
	int fd;		/* input file descriptor. */
	int allocated;	/* internal flag, whether or not buf was malloc()ed. */
};

int
readline_init(struct rl_data *data, int fd, void *buf, size_t bufcap);

void
readline_free(struct rl_data *data);

ssize_t
readline(struct rl_data *data, char *buf, size_t bufcap);

#endif
