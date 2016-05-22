#include <stdlib.h>	/* */
#include <stdio.h>	/* printf() */
#include <errno.h>	/* perror() */
#include <unistd.h>	/* close(), fork() */
#include <sys/wait.h>	/* waitpid() */

#include "lib/eve_parser.h"
#include "lib/eve_txn.h"

static int
bad_date(unsigned int year, unsigned int month, unsigned int day)
{
	if (year < 2006 || year > 3000) { /* Our dataset begins with 2006. */
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
	{	/* Preconditions */
		assert(str != NULL);
		assert(year != NULL);
		assert(month != NULL);
		assert(day != NULL);
	}
	{	/* Input format validation: */
		/* Confirm that datestring has the format 'YYYY-MM-DD' */
		if (str[4] != '-' || str[7] != '-') {
			return 1;
		}
	}
	{	/* Value parsing: */
		unsigned int i;
		unsigned int offsets[4] = {1000, 100, 10, 1};
		*year = *month = *day = 0;
		for (i = 0; i < 4; ++i) { /* Year is 4 digits */
			*year += (*str++ - '0') * offsets[i];
		}
		str++; /* Skip '-' */
		for (i = 0; i < 2; ++i) {
			*month += (*str++ - '0') * offsets[i + (4 - 2)];
		}
		str++; /* Skip '-' */
		for (i = 0; i < 2; ++i) {
			*day += (*str++ - '0') * offsets[i + (4 - 2)];
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
	FILE *fin = fdopen(infd, "r");
	char datebuf[12];
	char linebuf[500];
	eve_txn_parser parse_txn;

	{ /* Initialize the parse_txn pointer */
		unsigned int year, mon, day;
		fgets(datebuf, 12, fin);
		if (parse_date(datebuf, &year, &mon, &day)) {
			printf("Bad date linebuf.\n");
			return 3;
		}
		if ((parse_txn=eve_txn_parser_factory(year, mon, day))==NULL) {
			printf("Bad date: %u %u %u\n", year, mon, day);
			return 4;
		}
	}
	{ /* Get rid of the header linebuf and parse each transaction. */
		fgets(linebuf, 500, fin); /* Get rid of header. */
		printf("DEBUG: linebuf: %s\n", linebuf);
		while (fgets(linebuf, 500, fin)) {
			parse_handler(linebuf, outfd, parse_txn);
		}
		if (feof(fin)) {
			printf("Error reading: %s\n", datebuf);
			return -1;
		}
		printf("Finished file: %s\n", datebuf);
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
