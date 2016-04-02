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

#define BUFSIZE 16384

struct rl_data {
	char *buf;
	char *bptr;
	ssize_t blen;
	size_t bcap;
	int fd;
	int allocated;
};

static ssize_t
readline(struct rl_data *data, char *buf, size_t bufsize)
{
	char *bufptr = buf;

	assert(buf != NULL);
	assert(bufsize > 0);
	assert(data != NULL);
	assert(data->buf != NULL);
	assert(data->blen > 0);
	assert(data->blen < (ssize_t)data->bcap);


	while(--bufsize > 1) { /* We return the \n as well, unlike K&R */
		if (data->bptr - data->buf >= data->blen) {
			data->blen = read(data->fd, data->buf, data->bcap);
			data->bptr = data->buf;
			/* 0 is a valid return size, so shift error codes down by 1 */
			if (data->blen == 0 || data->blen == -1) {
				return data->blen - 1;
			}
		}

		if (*data->bptr == '\n') {
			*bufptr++ = *data->bptr++;
			break;
		}

		*bufptr++ = *data->bptr++;
	}

	*bufptr = '\0';
	return bufptr - buf;
}

static int
readline_init(struct rl_data *data, int fd, size_t buflen, void *buf)
{
	assert(data != NULL);
	assert(fd >= 0);
	assert(buflen > 0);

	if (buf != NULL) { /* Allow user to specify their own buffer. */
		data->buf = (char *)buf;
		data->allocated = 0;
	} else { /* Do it for them */
		data->buf = malloc(buflen);
		if (!data->buf) {
			return 1;
		}
		data->allocated = 1;
	}

	memset(data->buf, 0, data->bcap);

	data->bptr = data->buf;
	data->bcap = buflen;
	data->fd = fd;

	if ((data->blen = read(data->fd, data->buf, data->bcap)) == -1) {
		return 2;
	}

	assert(data->buf != NULL);
	assert(data->fd >= 0);
	assert(data->bcap > 0);

	return 0;
}

static void
readline_free(struct rl_data *data)
{
	assert(data != NULL);

	if (data->allocated) {
		free(data->buf);
	}
	data->bptr = data->buf = NULL;
	data->blen = data->bcap = data->fd = 0;

	return;
}

static unsigned int
pchr(char c)
{
	return (unsigned int)(c - '0');
}

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

	*year = pchr(str[0]) * 1000 + pchr(str[1]) * 100;
	*year += pchr(str[2]) * 10  + pchr(str[3]);
	*month = pchr(str[5]) * 10 + pchr(str[6]); /* skip '-' */
	*day = pchr(str[8]) * 10 + pchr(str[9]); /* skip '-' */

	if (*year < 2000 || *year > 3000 || *month < 1
			|| *month > 12 || *day < 1 || *day > 31) {
		return 2;
	}

	return 0;
}

int
main(void)
{
	char buf[2][BUFSIZE]; /* Buffers for input and output */
	char line[500]; /* Buffer for each line. */
	unsigned int year[2] = {0, 0}, month[2] = {0, 0}, day[2] = {0, 0};
	FILE *out = NULL;
	int rc = 0;
	ssize_t linelength = 0;
	size_t linecount = 0;
	Parser parser = NULL;
	struct raw_record rec;
	struct rl_data rl_data;

	if (!(out = fopen("./data/log", "ab"))) {
		printf("Failed to open log FILE.\n");
		rc = 1;
		goto fail_fopen;
	}

	if ((rc = readline_init(&rl_data, 0, BUFSIZE, buf[0]))) {
		goto fail_readline_init;
	}

	setvbuf(out, buf[1], _IOFBF, BUFSIZE);

	/* Read line by line to ease parsing complexity. */
	while ((linelength = readline(&rl_data, line, BUFSIZE)) >= 0) {
		/* linelength == 11 check is a slight optimization */
		if (linelength == 11) {
			if (parse_date(line, &year[1], &month[1], &day[1])) {
				printf("Bad line: %s\n", line);
				continue;
			}

			parser = parser_factory(year[1], month[1], day[1]);
			readline(&rl_data, line, BUFSIZE); /* Skip trailing hdr line. */

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
		switch(parser(line, &rec)) {
		case 0:
			fwrite(&rec, sizeof(rec), 1, out);
			break;
		case 1:
			printf("Bad time (%u, %u) at line %zu\n%s\n",
			    rec.issued, rec.rtime, linecount, line);
			break;
		case 2:
			printf("Bad bid %u at line %zu\n%s\n", rec.bid, linecount, line);
			break;
		case 3:
			printf("Bad range at line %zu\n%s\n", linecount, line);
			break;
		default:
			printf("Bad line at line: %zu\n%s\n", linecount, line);
			break;
		}
		linecount++;
	}

	if (linelength != -1) {
		rc = 1;
		printf("Error reading: %u-%u-%u\n", year[1], month[1], day[1]);
	}

	readline_free(&rl_data);
fail_readline_init:
	fclose(out);
fail_fopen:
	return (int)rc;
}
