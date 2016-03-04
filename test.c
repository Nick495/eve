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

#define FCNT 16

void create_path(char *buf, const char *dp, const char *name, const char *ext)
{
	memcpy(buf, dp, strlen(dp));
	memcpy(buf + strlen(dp), name, strlen(name));
	memcpy(buf + strlen(dp) + strlen(name), ext, strlen(ext) + 1);
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
	int fds[FCNT];

	uint32_t year, month, day;
	Parser parser;
	struct rawRecord record;
	int err;

	size_t i = 0;


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
	while ((err = scanf("%u-%u-%u\n", &year, &month, &day) == 3)) {
		parser = parser_factory(ejaday(year, month, day));

		while(parser(&record) == 0) {
		}
	}
	/* Write to files. */
	Parser p

	return EXIT_SUCCESS;

fail_open:
	for (; i > 0; --i) {
		close(fds[i - 1]);
	}

	return EXIT_FAILURE;
}
