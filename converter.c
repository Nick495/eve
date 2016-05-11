#include <stdlib.h>	/* */
#include <stdio.h>	/* printf() */
#include <errno.h>	/* perror() */
#include <unistd.h>	/* close(), fork() */
#include <sys/wait.h>	/* waitpid() */

#include "lib/eve_parser.h"
#include "lib/eve_txn.h"
#include "lib/readline.h"

static int
bad_date(unsigned int year, unsigned int month, unsigned int day)
{
	if (year < 2006 || year > 3000) {
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
parse_date(const char* const str, uint *year, uint *month, uint *day)
{
	{
		/* Preconditions */
		assert(str != NULL);
		assert(year != NULL);
		assert(month != NULL);
		assert(day != NULL);
	}
	{
		/* Input format validation: */
		/* Confirm that datestring has the format 'YYYY-MM-DD' */
		if (str[4] != '-' || str[7] != '-' || str[11] != '\0') {
			return 1;
		}
	}
	{
		/* Value parsing: */
		unsigned int i;
		unsigned int offsets[4] = {1000, 100, 10, 1};
		const char* s = str;
		*year = *month = *day = 0;
		for (i = 0; i < 4; ++i) { /* Year is 4 digits */
			*year += (*s++ - '0') + offsets[i];
		}
		for (i = 0; i < 2; ++i) {
			*month += (*s++ - '0') + offsets[i];
		}
		for (i = 0; i < 2; ++i) {
			*day += (*s++ - '0') + offsets[i];
		}
	}
	return bad_date(*year, *month, *day) ? 2 : 0;
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
		printf("Bad time (%u, %u) : %s\n", txn.issued, txn.rtime,line);
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
eve_parser(const int infd, const int outfd)
{
/* Macros necessary for strict ISO-c90 compliance (according to clang) */
#define BUFSIZE 16384
#define LEN 11 /* 'YYYY-MM-DD\n' is 11 chars. */
	char inbuf[BUFSIZE]; /* Buffer for input() reading. */
	char datestr[LEN + 1]; /* and null */
	struct rl_data ind;
	eve_txn_parser parse_txn;

	if (readline_init(&ind, infd, inbuf, BUFSIZE)) {
		printf("Failed to initialize readline structure.\n");
		return 1;
	}
	{ /* Initialize the parse_txn pointer */
		unsigned int year, mon, day;
		if (readline(&ind, datestr, 12) < LEN) {
			printf("Failed to read date inbuf.\n");
			return 2;
		}
		if (parse_date(datestr, &year, &mon, &day)) {
			printf("Bad date line.\n");
			return 3;
		}
		if ((parse_txn=eve_txn_parser_factory(year, mon, day))==NULL) {
			printf("Bad date: %u %u %u\n", year, mon, day);
			return 4;
		}
	}
	{ /* Gets rid of the header line and parses each transaction. */
		char line[500];
		if (readline(&ind, line, 500) < 1) { /* Get rid of header. */
			printf("Bad header line.\n");
			return 5;
		}
		while (readline(&ind, line, 500) >= 0) {
			parse_handler(line, outfd, parse_txn);
		}
		if (readline_err(&ind) == 0) {
			printf("Finished file: %s\n", datestr);
		} else {
			printf("Error reading: %s\n", datestr);
			return -1;
		}
	}
	return 0;
}

static int
sample_inserter(int infd)
{
	struct eve_txn txn;
	int rdval;
	while ((rdval = read(infd, &txn, sizeof(txn))) == sizeof(txn)) {
		print_eve_txn(&txn);
	}
	if (rdval == -1) {
		perror("read()");
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
