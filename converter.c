#include <stdlib.h>	/* */
#include <stdio.h>	/* fgets(), sscanf(), printf() */
#include <errno.h>	/* errno, perror() */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close(), fork() */
#include <string.h>	/* strlen() */
#include <sys/wait.h> /* waitpid() */

#include "lib/eve_parser.h"
#include "lib/eve_txn.h"
#include "lib/readline.h"
#include "lib/lmdb/libraries/liblmdb/lmdb.h"
#include "converter_lmdb.c"

typedef unsigned int uint;
static int
parse_date(const char *str, uint *year, uint *month, uint *day)
{
	assert(str != NULL);
	assert(year != NULL);
	assert(month != NULL);
	assert(day != NULL);

	if (str[4] != '-' || str[7] != '-' || str[11] != '\0') {
		return 1; /* Valid datestrings have the format 'YYYY-MM-DD' */
	}

	*year = (uint) ((str[0] - '0') * 1000 + (str[1] - '0') * 100
		+ (str[2] - '0') * 10 + (str[3] - '0'));
	*month = (uint)((str[5] - '0') * 10 + (str[6] - '0')); /* skip '-' */
	*day = (uint)((str[8] - '0') * 10 + (str[9] - '0')); /* skip '-' */

	if (*year < 2000 || *year > 3000 || *month < 1 || *month > 12
		|| *day < 1 || *day > 31) {
		return 2;
	}

	return 0;
}

static int
eve_parser(int infd, int outfd)
{
#define BUFSIZE 16384
	struct rl_data lineReader;
	char inbuf[BUFSIZE];
	char linebuf[500]; /* Must be larger than the size of every linebuf. */
	unsigned int year, month, day;
	eve_txn_parser parseline;
	ssize_t linelength = 0;
	struct eve_txn txn;

	if (readline_init(&lineReader, 0, inbuf, BUFSIZE)) {
		goto fail_readline_init;
	}
	if (readline(&lineReader, linebuf, 500) != 11) { /* 'YYYY-MM-DD\n' = 11*/
		printf("Failed to read date linebuf: %s\n", linebuf);
		goto fail_get_date;
	}
	if (parse_date(linebuf, &year, &month, &day)) {
		printf("Bad linebuf: %s\n", linebuf);
		goto fail_parse_date;
	}
	if ((parseline = eve_txn_parser_factory(year, month, day)) == NULL) {
		printf("Bad date: %u %u %u\n", year, month, day);
		goto fail_baddate;
	}
	if (readline(&lineReader, linebuf, 500) < 0) { /* Skip header line. */
		printf("Bad header: %s\n", linebuf);
		goto fail_get_header;
	}

	while ((linelength = readline(&lineReader, linebuf, 500)) >= 0) {
		switch(parseline(linebuf, &txn)) {
		case 0:
			write(outfd, &txn, sizeof(txn));
			break;
		case 1:
			printf("Bad time (%u, %u) : %s\n", txn.issued, txn.rtime, linebuf);
			break;
		case 2:
			printf("Bad bid %u : %s\n", txn.bid, linebuf);
			break;
		case 3:
			printf("Bad range : %s\n", linebuf);
			break;
		default:
			printf("Bad linebuf  %s\n", linebuf);
			break;
		}
	}

	if (linelength == -1) {
		printf("Finished file: %u-%u-%u\n", year, month, day);
	} else {
		printf("Error reading: %u-%u-%u\n", year, month, day);
		goto fail_badread;
	}

	readline_free(&lineReader);
	return 0;

fail_badread:
fail_baddate:
fail_get_header:
fail_parse_date:
fail_get_date:
	readline_free(&lineReader);
fail_readline_init:
	return 1;
}

int
main(void)
{
	int pipes[2];
	pid_t childpid;
	int rc;
	if (pipe(pipes)) {
		printf("Failed to make a pipe.\n");
		goto fail_pipe;
	}

	switch(childpid = fork()) {
	case -1:
		printf("Failed to fork().\n");
		goto fail_fork;
		break;
	case 0: /* child */
		close(pipes[1]);
		return lmdb_insert(pipes[0]);
	default: /* parent */
		close(pipes[0]);
		rc = eve_parser(1, pipes[1]); /* parse stdin */
		close(pipes[1]);
		waitpid(childpid, NULL, 0); /* Wait for (inserter) to complete. */
		return rc;
	}

fail_fork:
	close(pipes[0]);
	close(pipes[1]);
fail_pipe:
	return 1;
}
