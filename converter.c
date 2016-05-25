#include <stdio.h>	/* printf() */
#include <errno.h>	/* perror() */
#include <unistd.h>	/* close(), fork() */
#include <sys/wait.h>	/* waitpid() */
#include <string.h>	/* strcpy() strcat() */
#include <fcntl.h>	/* O_WRONLY */

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
 * Example 'output'. Prints txns to stdout.
*/
static int
sample_output(int infd)
{
	ssize_t rb;
	struct eve_txn txn;
	FILE* fout;
	{ /* Init fout */
		const char* const name = "./test_out";
		if (!(fout = fopen(name, "ab"))) {
			printf("Failed to open %s with error: %s\n",
				name, strerror(errno));
			return 1;
		}
	}
	/* TODO: Handle write errors */
	/* Write the eve_txns from infd, row-wise. */
	while ((rb = read(infd, &txn, sizeof(txn))) == sizeof(txn)) {
		fwrite((void *)&txn, sizeof(txn), 1, fout);
	}
	if (rb == -1) {
		perror("read()");
		return 1;
	}
	fclose(fout);
	return 0;
}

static int
sample_column_output(int infd)
{
	ssize_t rb;
	struct eve_txn txn;
	FILE* fouts[15];
	{ /* Initialize fouts */
		const char* const prefix = "./data/";
		const char* const names[15] = { "orderid", "regionid",
			"systemid", "stationid", "typeid", "bid", "price",
			"volmin", "volrem", "volent", "issued", "duration",
			"range", "reportedby", "reportedtime"
		};
		char buf[256];
		for (rb = 0; rb < 15; ++rb) {
			strcpy(buf, prefix);
			strcat(buf, names[rb]);
			fouts[rb] = fopen(buf, "ab");
			if (!fouts[rb]) {
				printf("Failed to open %s with error: %s\n",
				    names[rb], strerror(errno));
				for (; rb > 0; rb--) { fclose(fouts[rb - 1]); }
				return 1;
			}
		}
	}
	/* TODO: Error handle the writes, 'factor out the loop'. */
	/* Write the eve_txns from infd, column-wise. */
	while ((rb = read(infd, &txn, sizeof(txn))) == sizeof(txn)) {
	    fwrite((void *)&txn.orderID, sizeof(txn.orderID), 1, fouts[0]);
	    fwrite((void *)&txn.regionID, sizeof(txn.regionID), 1, fouts[1]);
	    fwrite((void *)&txn.systemID, sizeof(txn.systemID), 1, fouts[2]);
	    fwrite((void *)&txn.stationID, sizeof(txn.stationID), 1, fouts[3]);
	    fwrite((void *)&txn.typeID, sizeof(txn.typeID), 1, fouts[4]);
	    fwrite((void *)&txn.bid, sizeof(txn.bid), 1, fouts[5]);
	    fwrite((void *)&txn.price, sizeof(txn.price), 1, fouts[6]);
	    fwrite((void *)&txn.volMin, sizeof(txn.volMin), 1, fouts[7]);
	    fwrite((void *)&txn.volRem, sizeof(txn.volRem), 1, fouts[8]);
	    fwrite((void *)&txn.volEnt, sizeof(txn.volEnt), 1, fouts[9]);
	    fwrite((void *)&txn.issued, sizeof(txn.issued), 1, fouts[10]);
	    fwrite((void *)&txn.duration, sizeof(txn.duration), 1, fouts[11]);
	    fwrite((void *)&txn.range, sizeof(txn.range), 1, fouts[12]);
	    fwrite((void *)&txn.reportedby,sizeof(txn.reportedby),1,fouts[13]);
	    fwrite((void *)&txn.rtime, sizeof(txn.rtime), 1, fouts[14]);
	}
	if (rb == -1) {
		perror("read()");
		return 1;
	}
	for (rb = 0; rb < 15; ++rb) {
		fclose(fouts[rb]);
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
		return sample_output(pipes[0]);
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
