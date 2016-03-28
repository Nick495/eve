/*
 * TODO: Change struct rawRecord from a struct to a tuple (a contiguous array
 * with tokens which point into the array, as discussed earlier).
 * Also clean up the interface for creating these files. Manually opening
 * each file, and hard-coding the indexes is really ghetto.
*/
#include <stdlib.h>	/* */
#include <stdio.h>	/* fgets(), sscanf(), printf() */
#include <errno.h>	/* errno, perror() */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close() */

#include "eve_parser.h"

#define BUFSIZE 65536

int
main(void)
{
	char buf[BUFSIZE][3]; /* Buffers for input, output, and fgets() */
	unsigned int year[2] = {0, 0}, month[2] = {0, 0}, day[2] = {0, 0};
	FILE *out = NULL;
	size_t linecount = 0;
	Parser parser = NULL;
	struct raw_record rec;

	if (!(out = fopen("./data/log", "ab"))) {
		printf("Failed to open log FILE.\n");
		exit(EXIT_FAILURE);
	}

	setvbuf(stdin, buf[0], _IOFBF, BUFSIZE);
	setvbuf(out, buf[1], _IOFBF, BUFSIZE);

	/* Read line by line to ease parsing complexity. */
	while (fgets(buf[3], BUFSIZE, stdin) != NULL) {
		if (sscanf(buf[3], "%u-%u-%u\n",
			    &(year[1]), &(month[1]), &(day[1])) == 3) {
			parser = parser_factory(year[1], month[1], day[1]);

			/* Skip header line. */
			fgets(buf[3], BUFSIZE, stdin);

			/* Print the last file, which we finished. */
			/* If we don't have a last file, don't bother. */
			if (year[0] != 0 && month[0] != 0 && day[0] != 0) {
				printf("Finished file: %u-%u-%u\n",
					year[0], month[0], day[0]);
			}

			/* Update the last file attributes */
			year[0] = year[1];
			month[0] = month[1];
			day[0] = day[1];
			linecount = 0;
		}

		switch(parser(buf[3], &rec)) {
		case 0:
			fwrite(&rec, sizeof(rec), 1, out);
			break;
		case 1:
			printf("Bad time (%u, %u) at line %zu\n",
			    rec.issued, rec.rtime, linecount);
			break;
		case 2:
			printf("Bad bid %u at line %zu\n", rec.bid, linecount);
			break;
		case 3:
			printf("Bad range at line %zu\n%s\n",linecount,buf[3]);
			break;
		default:
			printf("Bad line at line: %zu\n", linecount);
		}
		linecount++;
	}

	if (ferror(stdin)) {
		printf("Error reading: %u-%u-%u\n", year[1], month[1], day[1]);
	} else {
		printf("Finished file: %u-%u-%u\n", year[1], month[1], day[1]);
	}

	fclose(out);
	return EXIT_SUCCESS;
}
