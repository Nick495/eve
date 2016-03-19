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
#define BUFSIZE 16384

static int create_path(char *buf, const size_t blen,
    const char *dp, const char *name, const char *ext)
{
	const size_t dlen = strlen(dp);
	const size_t nlen = strlen(name);
	const size_t elen = strlen(ext);

	if (blen < dlen + nlen + elen + 1) { /* And null terminator. */
		return 1;
	}

	memcpy(buf, dp, dlen);
	memcpy(buf + dlen, name, nlen);
	memcpy(buf + dlen + nlen, ext, elen + 1); /* And null terminator. */
	return 0;
}

static int write_token(FILE *f, token *t)
{
	if (fwrite(t->ptr, t->len, 1, f) < 1) {
		return 1;
	}

	return 0;
}

static int write_tokens(FILE **fs, token *tokens, const size_t tokenCount)
{
	size_t i;
	for (i = 0; i < tokenCount; ++i) {
		if (write_token(fs[i], &(tokens[i]))) {
#if 0
			printf("DEBUG: %zu, %zu\n", i, tokens[i].len);
			perror("fwrite:");
#endif
			return 1;
		}
	}

	return 0;
}

static void cmp_tokens(token *a, token *b, const size_t tokenCount,
    const char **names, const char *str)
{
	size_t i = 0;
	int flag = 0;

	for (i = 0; i < tokenCount; ++i) {
		if (memcmp(a[i].ptr, b[i].ptr, a[i].len)) {
			flag = 1;
			printf("DEBUG: Failed to match field %s | ",
			    names[i]);
			switch(i) {
			case 0: /* orderid */
			case 6: /* price: */
			case 13: /* reportedby */
				printf("%llu , %llu\n",
				    *(uint64_t *)a[i].ptr,
				    *(uint64_t *)b[i].ptr);
				break;
			case 5: /* bid */
			case 12: /* range */
				printf("%d , %d\n",
				    *(int8_t *)a[i].ptr,
				    *(int8_t *)b[i].ptr);
				break;
			case 11: /* duration */
				printf("%d , %d\n",
				    *(uint8_t *)a[i].ptr,
				    *(uint8_t *)b[i].ptr);
				break;
			default:
				printf("%d , %d\n",
				    *(uint32_t *)a[i].ptr,
				    *(uint32_t *)b[i].ptr);
				break;
			}
		}
	}

	if (flag) {
		printf("%s\n", str);
	}
}

int main(void)
{
	const char *dirpath = "./data/";
	const char *ext = ".db";
	const char *names[FCNT] = {
		"orderid", "regionid", "systemid", "stationid", "typeid",
		"bid", "price", "volmin", "volrem", "volent", "issued",
		"duration", "range", "reportedby", "reportedtime", "deleted"
	};
	char buf[BUFSIZE]; /* Arbitrary length, fix later. */
	uint32_t year, month, day;
	uint32_t oyear = 0, omonth = 0, oday = 0;
	FILE* fs[FCNT];
	token tokens[FCNT];
	Parser parser;
	ssize_t i = 0;
	size_t linecount;

	/* Open necessary files. */
	for (i = 0; i < FCNT; ++i) {
		if (create_path(buf, BUFSIZE, dirpath, names[i], ext)) {
			printf("Buflen too short.\n");
			goto fail_open;
		}

		fs[i] = fopen(buf, "ab");
		if (fs[i] == NULL) {
			printf("%s\n", buf);
			perror("Open");
			goto fail_open;
		}

		if (init_token(&(tokens[i]))) {
			printf("%s\n", buf);
			goto fail_init_token;
		}
	}

	/* Initially none of them are deleted. */
	memset(tokens[15]._buf, 0, sizeof(uint8_t));
	tokens[15].ptr = tokens[15]._buf;
	tokens[15].len = sizeof(uint8_t);

	/* Read from stdin and write to files. */
	while (fgets(buf, BUFSIZE, stdin) != NULL) {
		if (sscanf(buf, "%u-%u-%u\n", &year, &month, &day) == 3) {
			parser = parser_factory(year, month, day);

			/* Skip header line. */
			fgets(buf, BUFSIZE, stdin);

			if (oyear != 0 && omonth != 0 && oday != 0) {
				/* Print the last file, which we finished. */
				printf("Finished file: %u-%u-%u\n", oyear,
				    omonth, oday);
			}

			/* Update the last file attributes */
			oyear = year;
			omonth = month;
			oday = day;
			linecount = 0;
			continue;
		}

		i = parser(buf, tokens, FCNT);
		switch(i) {
		case 0:
			write_tokens(fs, tokens, FCNT);
			break;
		case 1:
			printf("Bad time at line %zu\n", linecount);
			break;
		default:
			printf("Bad line at line: %zu, %zu\n%s\n",
			    i, linecount, buf);
		}
		linecount++;
	}

	if (ferror(stdin)) {
		printf("Error reading: %u-%u-%u\n", year, month, day);
	} else {
		printf("Finished file: %u-%u-%u\n", year, month, day);
	}

	for (i = 0; i < FCNT; ++i) {
		fclose(fs[i]);
	}
	return EXIT_SUCCESS;

fail_init_testToken:
fail_init_token:
fail_open:
	for (; i > 0; --i) {
		fclose(fs[i - 1]);
	}

	return EXIT_FAILURE;
}
