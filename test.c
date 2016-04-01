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
#include <string.h>	/* strlen() */

#include "eve_parser.h"

#define BUFSIZE 65536

static int
parse_date(const char *str,
	unsigned int *year, unsigned int *month, unsigned int *day)
{
	assert(str != NULL);
	assert(year != NULL);
	assert(month != NULL);
	assert(day != NULL);

	if (str[4] != '-' || str[7] != '-') {
		return 1;
	}

	if (str[11] != '\0') {
		return 1;
	}

	*year = (*str++ - '0') * 1000;
	*year += (*str++ -'0') * 100;
	*year += (*str++ - '0') * 10;
	*year += (*str++ - '0');
	str++; /* skip '-' */

	*month = (*str++ - '0') * 10;
	*month += (*str++ - '0');
	str++; /* skip '-' */

	*day = (*str++ - '0') * 10;
	*day = (*str++ - '0');

	if (*year < 2000 || *year > 3000) {
		return 2;
	} else if (*month < 1 || *month > 12) {
		return 2;
	} else if (*day < 1 || *day > 31) {
		return 2;
	}

	return 0;
}

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
		switch(parse_date(buf[3], &year[1], &month[1], &day[1])) {
		case 1: /* Normal line, pass to parser. */
			break;
		case 2: /* Bad datestring */
			printf("Bad datestring: %s\n", buf[3]);
			continue;
		case 0: /* Handle datestring */
			parser = parser_factory(year[1], month[1], day[1]);

			fgets(buf[3], BUFSIZE, stdin); /* Skip header line. */

			/* Print the last file, which we finished. */
			/* If we don't have a last file, don't bother. */
			if (year[0] != 0 && month[0] != 0 && day[0] != 0) {
				printf("Finished file: %u-%u-%u\n", year[0], month[0], day[0]);
			}

			/* Update the last file attributes */
			year[0] = year[1];
			month[0] = month[1];
			day[0] = day[1];
			linecount = 0;
			continue;
		}
		assert(parser != NULL);

		switch(parser(buf[3], &rec)) {
		case 0:
			fwrite(&rec, sizeof(rec), 1, out);
			break;
		case 1:
			printf("Bad time (%u, %u) at line %zu\n%s\n",
			    rec.issued, rec.rtime, linecount, buf[3]);
			break;
		case 2:
			printf("Bad bid %u at line %zu\n%s\n", rec.bid, linecount, buf[3]);
			break;
		case 3:
			printf("Bad range at line %zu\n%s\n", linecount, buf[3]);
			break;
		default:
			printf("Bad line at line: %zu\n%s\n", linecount, buf[3]);
			break;
		}
		linecount++;
	}

	if (ferror(stdin)) {
		printf("Error reading: %u-%u-%u\n", year[1], month[1], day[1]);
	}

	fclose(out);
	return EXIT_SUCCESS;
}
