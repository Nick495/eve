#include <stdlib.h>	/* */
#include <stdio.h>	/* fgets(), sscanf(), printf() */
#include <errno.h>	/* errno, perror() */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close(), fork() */
#include <string.h>	/* strlen() */
#include <sys/wait.h>	/* waitpid() */

#include "lib/eve_parser.h"
#include "lib/eve_txn.h"
#include "lib/readline.h"

static int
bad_date(unsigned int year, unsigned int month, unsigned int day)
{
	if (year < 2000 || year > 3000) {
		return 1;
	} else if (month < 1 || month > 12) {
		return 1;
	} else if (day < 1 || day > 31) {
		return 1;
	}
	return 0;
}

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

	*year = (uint)((str[0] - '0') * 1000 + (str[1] - '0') * 100
		+ (str[2] - '0') * 10 + (str[3] - '0'));
	*month = (uint)((str[5] - '0') * 10 + (str[6] - '0')); /* skip '-' */
	*day = (uint)((str[8] - '0') * 10 + (str[9] - '0')); /* skip '-' */

	if (bad_date(*year, *month, *day)) {
	    return 2;
	}
	return 0;
}

static void
parse_handler(char *line, int outfd, eve_txn_parser parse_txn)
{
	struct eve_txn txn;
	switch(parse_txn(line, &txn)) {
	case 0:
		write(outfd, &txn, sizeof(txn));
		break;
	case 1:
		printf("Bad time (%u, %u) : %s\n", txn.issued,txn.rtime, line);
		break;
	case 2:
		printf("Bad bid %u : %s\n", txn.bid, line);
		break;
	case 3:
		printf("Bad range : %s\n", line);
		break;
	default:
		printf("Bad input  %s\n", line);
		break;
	}
	return;
}

static int
eve_parser(int infd, int outfd)
{
#define BUFSIZE 16384
	char line[500]; /* Buffer for actual line. */
	char inbuf[BUFSIZE] = {0}; /* Buffer for input() reading. */
	struct rl_data ind;
	unsigned int year, month, day;
	eve_txn_parser parse_txn;
	int rc = 0;

	if (readline_init(&ind, infd, inbuf, BUFSIZE)) {
		printf("Failed to initialize readline structure.\n");
		return 1;
	}
	/* Read 'YYYY-MM-DD\n' (11 chars) line which tells us how to parse. */
	if ((rc = readline(&ind, line, 500) < 11)) {
		printf("Failed to read date inbuf.\n");
		return 2;
	}
	if (parse_date(line, &year, &month, &day)) {
		printf("Bad date line.\n");
		return 3;
	}
	if ((parse_txn = eve_txn_parser_factory(year, month, day)) == NULL) {
		printf("Bad date: %u %u %u\n", year, month, day);
		return 4;
	}
	if (readline(&ind, line, 500) < 1) {
		printf("Bad header line.\n");
		return 5;
	}
	while (readline(&ind, line, 500) >= 0) {
		parse_handler(line, outfd, parse_txn);
	}
	if (readline_err(&ind) == 0) {
		printf("Finished file: %u-%u-%u\n", year, month, day);
	} else {
		printf("Error reading: %u-%u-%u\n", year, month, day);
		return -1;
	}
	return 0;
}

static int
sample_inserter(int infd)
{
	struct eve_txn txn;
	while (read(infd, &txn, sizeof(txn)) == sizeof(txn)) {
		print_eve_txn(&txn);
	}
	return 0;
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
	case 0: /* child */
		close(pipes[1]);
		return sample_inserter(pipes[0]);
	default: /* parent */
		close(pipes[0]);
		rc = eve_parser(STDIN_FILENO, pipes[1]); /* parse stdin */
		close(pipes[1]);
		waitpid(childpid, NULL, 0);
		return rc;
	}

fail_fork:
	close(pipes[0]);
	close(pipes[1]);
fail_pipe:
	return 1;
}
