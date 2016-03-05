/*
 * TODO: Change struct rawRecord from a struct to a tuple (a contiguous array
 * with tokens which point into the array, as discussed earlier).
 * Also clean up the interface for creating these files. Manually opening
 * each file, and hard-coding the indexes is really ghetto.
*/
#include <stdlib.h>	/* */
#include <stdio.h>	/* printf() */
#include <errno.h>	/* errno, perror() */
#include <string.h>	/* memcpy() */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close() */
#include <assert.h>	/* assert() */

#include "token.h"
#include "eve_parser.h"

#define FCNT 16

void create_path(char *buf, const char *dp, const char *name, const char *ext)
{
	memcpy(buf, dp, strlen(dp));
	memcpy(buf + strlen(dp), name, strlen(name));
	memcpy(buf + strlen(dp) + strlen(name), ext, strlen(ext) + 1);
}

int write_tokens(int *fds, token *tokens, const size_t tokenCount)
{
	size_t i;
	size_t len;
	for (i = 0; i < tokenCount; ++i) {
		len = write(fds[i], tokens[i].ptr, tokens[i].len);
		assert(len == tokens[i].len);
	}

	return 0;
}

int main(void)
{
	const char *dirpath = "./data/";
	const char *ext = ".db";
	const char *names[FCNT] = {
		"orderid", "price", "reportedby", "issued", "regionid",
		"systemid", "stationid", "typeid", "volmin", "volrem",
		"volent", "reportedtime", "duration", "range", "bid", "deleted"
	};
	char buf[256]; /* Arbitrary length, fix later. */
	uint32_t year, month, day;
	int fds[FCNT];
	token tokens[FCNT];
	Parser parser;
	ssize_t i = 0;


	/* Open necessary files. */
	for (i = 0; i < FCNT; ++i) {
		create_path(buf, dirpath, names[i], ext);

		fds[i] = open(buf, O_WRONLY|O_APPEND|O_CREAT, 0644);
		if (fds[i] < 0) {
			printf("%s\n", buf);
			perror("Open");
			goto fail_open;
		}
	}

	/* Read from stdin and write to files. */
	while ((i = scanf("%u-%u-%u\n", &year, &month, &day) == 3)) {

		/* Parser depends upon the input's creation date. */
		parser = parser_factory(year, month, day);

		/* Parse the whole file. */
		while ((i = parser(tokens, FCNT)) >= 0) {
			if (!i) {
				write_tokens(fds, tokens, FCNT);
			}
		}

		printf("Finished file: %u-%u-%u\n", year, month, day);
	}

	return EXIT_SUCCESS;

fail_open:
	for (; i > 0; --i) {
		close(fds[i - 1]);
	}

	return EXIT_FAILURE;
}
