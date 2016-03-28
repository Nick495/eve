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

#define BUFSIZE 16384

int main(void)
{
	char buf[BUFSIZE]; /* Arbitrary length, fix later. */
	unsigned int year[2] = {0, 0}, month[2] = {0, 0}, day[2] = {0, 0};
	size_t linecount = 0;
	Parser parser = NULL;
	struct raw_record rec;

	/* Read a line from stdin and parse and write. */
	while (fgets(buf, BUFSIZE, stdin) != NULL) {
		if (sscanf(buf, "%u-%u-%u\n",
			    &(year[1]), &(month[1]), &(day[1])) == 3) {
			parser = parser_factory(year[1], month[1], day[1]);

			/* Skip header line. */
			fgets(buf, BUFSIZE, stdin);

			/* Print the last file, which we finished. */
			printf("Finished file: %u-%u-%u\n",
			    year[0], month[0], day[0]);

			/* Update the last file attributes */
			year[0] = year[1];
			month[0] = month[1];
			day[0] = day[1];
			linecount = 0;
		}

		switch(parser(buf, &rec)) {
		case 0:
			/* TODO: Write */
			fwrite(&rec, sizeof(rec), 1, stdout);
			break;
		case 1:
			printf("Bad time at line %zu\n", linecount);
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

	return EXIT_SUCCESS;
}
