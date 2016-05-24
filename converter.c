#include <stdio.h>	/* printf() */
#include <errno.h>	/* perror() */
#include <unistd.h>	/* close(), fork() */
#include <sys/wait.h>	/* waitpid() */

#include "lib/eve_parser.h"
#include "lib/eve_txn.h"

/*
 * Parses the given line and writes appropriate error messages.
 * Convienence function which makes the eve_parser code look better.
*/
static void
parse_errhandler(const char *line, int outfd, const eve_txn_parser txn_parse)
{
	struct eve_txn txn;
	switch(txn_parse(line, &txn)) {
	case 0:
		write(outfd, &txn, sizeof(txn));
		return;
	case 1:
		printf("Bad time (%u, %u) : %s\n", txn.issued,txn.rtime,line);
		break;
	case 2:
		printf("Bad bid %u : %s", txn.bid, line);
		break;
	case 3:
		printf("Bad range : %s", line);
		break;
	default:
		printf("Bad input : %s", line);
		break;
	}
	return;
}

/*
 * Parses txns from the infd.
*/
static int
eve_parser(const int infd, const int outfd)
{
	FILE *fin = fdopen(infd, "r");
	char datebuf[12];
	eve_txn_parser parse_txn;

	{ /* Parse file date. */
		unsigned int year, mon, day;
		/* Read in 'YYYY-MM-DD' datestring header. */
		fgets(datebuf, 12, fin);
		year = (datebuf[0] - '0') * 1000 + (datebuf[1] - '0') * 100
			+ (datebuf[2] - '0') * 10 + (datebuf[3] - '0');
		assert(year >= 2006 && year <= 2020);
		mon = (datebuf[5] - '0') * 10 + (datebuf[6] - '0');
		assert(mon >= 1 && mon <= 12);
		day = (datebuf[8] - '0') * 10 + (datebuf[9] - '0');
		assert(day >= 1 && day <= 31);
		/* File format changes over time, so init it with the date. */
		parse_txn = eve_txn_parser_strategy(year, mon, day);
	}
	{ /* Parse transactions. */
		char linebuf[500]; /* > max line length + 1. */
		fgets(linebuf, 500, fin); /* Get rid of header line. */
		while (fgets(linebuf, 500, fin)) {
			parse_errhandler(linebuf, outfd, parse_txn);
		}
	}
	{ /* Error handling and reporting */
		if (!feof(fin)) {
			printf("Error reading: %s\n", datebuf);
			return -1;
		}
		printf("Finished file: %s\n", datebuf);
	}
	return 0;
}

/*
 * Example 'inserter'. Prints txns to stdout.
*/
static int
sample_inserter(int infd)
{
	ssize_t rb;
	struct eve_txn txn;
	while ((rb = read(infd, &txn, sizeof(txn))) == sizeof(txn)) {
		print_eve_txn(&txn);
	}
	if (rb == -1) {
		perror("read()");
	}
	return 0;
}

int
main(void)
{
	int rc, pipes[2];
	pid_t childpid;
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
