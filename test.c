#include <stdlib.h>	/* */
#include <stdio.h>	/* fgets(), sscanf(), printf() */
#include <errno.h>	/* errno, perror() */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close(), fork() */
#include <string.h>	/* strlen() */

#include "eve_parser.h"
#include "lz4/lib/lz4.h"

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
readline(struct rl_data *data, char *buf, size_t bufcap)
{
	assert(buf != NULL);
	assert(bufcap > 0);
	assert(data != NULL);
	assert(data->buf != NULL);
	assert(data->blen > 0);
	assert(data->blen <= (ssize_t)data->bcap);

	char *bufptr = buf;

	while(--bufcap > 1) { /* We return the '\n' as well, unlike K&R */
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
readline_init(struct rl_data *data, int fd, void *buf, size_t bufcap)
{
	assert(data != NULL);
	assert(fd >= 0);
	assert(bufcap > 0);

	if (buf != NULL) { /* Allow user to specify their own buffer. */
		data->buf = (char *)buf;
		data->allocated = 0;
	} else { /* Do it for them */
		data->buf = malloc(bufcap);
		if (!data->buf) {
			return 1;
		}
		data->allocated = 1;
	}

	data->bptr = data->buf;
	data->bcap = bufcap;
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

typedef unsigned int uint;
static int
parse_date(const char *str, uint *year, uint *month, uint *day)
{
	assert(str != NULL);
	assert(year != NULL);
	assert(month != NULL);
	assert(day != NULL);

	if (str[4] != '-' || str[7] != '-' || str[11] != '\0') {
		return 1; /* Valid datestrings have the format 'YYYY-MM-DD' */
	}

	*year = (uint) ((str[0] - '0') * 1000 + (str[1] - '0') * 100
		+ (str[2] - '0') * 10 + (str[3] - '0'));
	*month = (uint)((str[5] - '0') * 10 + (str[6] - '0')); /* skip '-' */
	*day = (uint)((str[8] - '0') * 10 + (str[9] - '0')); /* skip '-' */

	if (*year < 2000 || *year > 3000 || *month < 1 || *month > 12
		|| *day < 1 || *day > 31) {
		return 2;
	}

	return 0;
}

#define PAGESIZE 16384
#define HEADERSIZE 2
struct queue {
	char *page;
	void *data;
	/* use and cap are ints instead of size_t's because of lz4. */
	int fd;
	unsigned int dUse;
	unsigned int dCap;
	unsigned int eleSize;
	unsigned int pUse;
	unsigned int pSize;
	unsigned int pEleCount;
};

static int
queue_init(struct queue *q, int fd, unsigned int size, unsigned int bufCount)
{
	assert(fd >= 0);
	assert(size > 0);
	assert(bufCount > 0);

	if ((q->data = malloc(size * bufCount)) == NULL) {
		return 1;
	}
	if ((q->page = malloc(PAGESIZE)) == NULL) {
		return 1;
	}
	q->fd = fd;
	q->dUse = q->pEleCount = 0;
	q->dCap = bufCount;
	q->eleSize = size;
	q->pUse = HEADERSIZE; /* Leave room for page header. */
	q->pSize = PAGESIZE;
	q->pEleCount = 0;

	assert(q->pUse <= q->pSize);
	assert(q->dUse < q->dCap);
	return 0;
}

static void
queue_free(struct queue *q)
{
	free(q->data);
	free(q->page);
	return;
}

static int
queue_write(struct queue *q)
{
	assert(q->pUse <= q->pSize);

	/* Write header and page, then reset pUse */
	q->page[0] = (char)(q->pEleCount << 8);
	q->page[1] = (char)(q->pEleCount << 0);
	/* Add error handling. */
	write(q->fd, q->page, q->pSize);
	q->pUse = HEADERSIZE;
	q->pEleCount = 0;

	assert(q->pUse <= q->pSize);
	return 0;
}

/* Compress existing elements in the queue. */
static int
queue_compress(struct queue *q)
{
	const unsigned int lower_limit = 11; /* Comes from lz4, refactor. */
	const unsigned int buffer_len = (q->dUse * q->eleSize);
	int bytes_consumed = buffer_len; /* Try to fit all the bytes. */
	uint16_t eleCount = 0;

	/* Bytes taken in page by LZ4 compression. */
	int bytes_taken = LZ4_compress_destSize(q->data, q->page + q->pUse,
		&bytes_consumed, q->pSize - q->pUse);

	assert(q->pUse <= q->pSize);

	if (bytes_taken == 0) {
		return -1;
	}

	if (bytes_consumed < q->eleSize) {
		/* Below, we try to estimate how much space we have to leave in the
		 * buffer to ensure that we can keep compressing stuff. Sometimes
		 * our estimate will be wrong, so we have to handle that case. */
		queue_write(q);
		assert(q->pUse < q->pSize);
		return queue_compress(q);
	}

	if ((unsigned int)bytes_consumed % q->eleSize != 0) {
		/* We don't want our integers cut on page boundaries, So we tell lz4
		 * that our buffer is only as long as the number of integers it can
		 * successfully compress.
		*/
		bytes_consumed -= bytes_consumed % q->eleSize;
		bytes_taken = LZ4_compress_destSize(q->data,
			q->page + q->pUse , &bytes_consumed, q->pSize - q->pUse);
		if (bytes_taken == 0) {
			return -1;
		}
	}

	eleCount = (uint16_t)(bytes_consumed / q->eleSize);
	q->pEleCount += eleCount;
	q->dUse -= eleCount;
	q->pUse += bytes_taken;

	if (q->pSize - q->pUse <= lower_limit) {
		queue_write(q);
	}

	memmove(q->data, (char *)q->data+bytes_consumed,buffer_len-bytes_consumed);

	assert(q->pUse <= q->pSize);
	return 0;
}

static int
queue_push(struct queue *q, void *data)
{
	if (q->dUse < q->dCap) {
		memcpy((char *)q->data + q->dUse * q->eleSize, data, q->eleSize);
		q->dUse++;
		return 0;
	}

	if (queue_compress(q)) {
		return 1;
	} else {
		return queue_push(q, data);
	}
}

static int
queue_commit(struct queue *q)
{
	if (q->dUse == 0 && q->pUse == 0) { /* All data has been flushed. */
		return 0;
	}

	if (q->dUse == 0) { /* All data is in the compressed buffer. */
		return queue_write(q);
	}

	/* Otherwise, we have data that needs to be compressed. */
	if (q->pUse == q->pSize) {
		if (queue_write(q)) {
			return 1;
		}
	}
	if (queue_compress(q)) {
		return 1;
	}
	return queue_commit(q);
}

/* the *_arr_* functions are very long, but basically exist only to make
 * main() shorter. Our Raw_Record struct has 15 members, and each one is
 * sent to a different file (in column-store fashion). Therefore, we have to
 * open 15 files and init 15 queues. When we push we push to all of them.
 * It's quite laborious and perhaps there's a better method.
*/
static int
queue_arr_init(struct queue *q, struct Raw_Record rec, unsigned int bufsize,
	int *fds)
{
	int i = 0;
	if (queue_init(q + i, fds[i], sizeof(rec.orderID), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.price), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.reportedby), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.regionID), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.systemID), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.stationID), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.typeID), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.volMin), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.volRem), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.volEnt), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.issued), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.rtime), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.duration), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.range), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	if (queue_init(q + i, fds[i], sizeof(rec.bid), bufsize)) {
		goto fail_queue_init;
	}
	++i;
	return 0;

fail_queue_init:
	for (; i > 0; --i) {
		free(q + i - 1);
	}
	return 1;
}

static int
queue_arr_push(struct queue *q, struct Raw_Record rec)
{
	int i = 0;
	if (queue_push(q + i++, &rec.orderID)) {
		printf("Failed to push orderID.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.price)) {
		printf("Failed to push price.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.reportedby)) {
		printf("Failed to push reportedby.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.regionID)) {
		printf("Failed to push regionID.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.systemID)) {
		printf("Failed to push systemID.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.stationID)) {
		printf("Failed to push stationID.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.typeID)) {
		printf("Failed to push typeID.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.volMin)) {
		printf("Failed to push volMin.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.volRem)) {
		printf("Failed to push volRem.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.volEnt)) {
		printf("Failed to push volEnt.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.issued)) {
		printf("Failed to push issued.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.rtime)) {
		printf("Failed to push rtime.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.duration)) {
		printf("Failed to push duration.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.range)) {
		printf("Failed to compress range.\n");
		return 1;
	}
	if (queue_push(q + i++, &rec.bid)) {
		printf("Failed to compress bid.\n");
		return 1;
	}
	return 0;
}

static int
queue_arr_commit(struct queue *q)
{
	int i;
	for (i = 0; i < 15; ++i) {
		if (queue_commit(q + i)) {
			return 1;
		}
	}

	return 0;
}

static int
fd_arr_init(int *fds)
{
	char buf[200];
	char names[15][25] = { "orderid.db", "price.db", "reportedby.db",
		"regionid.db", "systemid.db", "stationid.db", "typeid.db", "volmin.db",
		"volrem.db", "volent.db", "issued.db", "rtime.db", "duration.db",
		"range.db", "bid.db" };
	size_t i;

	for (i = 0; i < 15; ++i) {
		strcpy(buf, "./data/");
		strcat(buf, names[i]);
		fds[i] = open(buf, O_WRONLY|O_APPEND|O_CREAT, 0644);
		if (fds[i] < 0) {
			goto fail_fds_open;
		}
	}

	return 0;

fail_fds_open:
	while (i > 0) {
		close(fds[i - 1]);
	}

	return 1;
}

/* Debug */
void
print_Raw_Record(struct Raw_Record *r)
{
	printf(" %llu ", r->orderID);
	printf(" %u ", r->regionID);
	printf(" %u ", r->systemID);
	printf(" %u ", r->stationID);
	printf(" %u ", r->typeID);
	printf(" %u ", r->bid);
	printf(" %llu ", r->price);
	printf(" %u ", r->volMin);
	printf(" %u ", r->volRem);
	printf(" %u ", r->volEnt);
	printf(" %u ", r->issued);
	printf(" %u ", r->duration);
	printf(" %u ", r->range);
	printf(" %llu ", r->reportedby);
	printf(" %u \n", r->rtime);

	return;
}

int
main(void)
{
	char buf[BUFSIZE]; /* Buffer for input */
	ssize_t linelength = 0;
	unsigned int year, month, day;
	Parser parser;
	struct rl_data rlData;
	char line[500];
	struct Raw_Record rec;
	struct queue queues[15];
	int fds[15];

	if (fd_arr_init(fds)) {
		goto fail_fd_arr_init;
	}

	if (queue_arr_init(queues, rec, 10000, fds)) {
		goto fail_queue_arr_init;
	}

	if (readline_init(&rlData, 0, buf, BUFSIZE)) {
		goto fail_readline_init;
	}

	if (readline(&rlData, line, BUFSIZE) != 11) {/* The date format 11 chars*/
		printf("Failed to read date line: %s\n", line);
		goto fail_get_date;
	}

	if (parse_date(line, &year, &month, &day)) {
		printf("Bad line: %s\n", line);
		goto fail_parse_date;
	}

	if (readline(&rlData, line, 500) < 0) { /* Skip trailing hdr line. */
		printf("Bad header: %s\n", line);
		goto fail_get_header;
	}

	if ((parser = parser_factory(year, month, day)) == NULL) {
		printf("Bad date: %u %u %u\n", year, month, day);
		goto fail_baddate;
	}
	assert(parser != NULL);

	/* Read line by line to ease parsing complexity. */
	while ((linelength = readline(&rlData, line, 500)) >= 0) {
		switch(parser(line, &rec)) {
		case 0:
			if (queue_arr_push(queues, rec)) {
				goto fail_queue_arr_push;
			}
			break;
		case 1:
			printf("Bad time (%u, %u) : %s\n", rec.issued, rec.rtime, line);
			break;
		case 2:
			printf("Bad bid %u : %s\n", rec.bid, line);
			break;
		case 3:
			printf("Bad range : %s\n", line);
			break;
		default:
			printf("Bad line  %s\n", line);
			break;
		}
	}

	if (queue_arr_commit(queues)) {
		goto fail_queues_commit;
	}

	if (linelength == -1) {
		printf("Finished file: %u-%u-%u\n", year, month, day);
	} else {
		printf("Error reading: %u-%u-%u\n", year, month, day);
		goto fail_badread;
	}

	readline_free(&rlData);
	for (linelength = 0; linelength < 15; ++linelength) {
		queue_free(&queues[linelength]);
	}
	for (linelength = 0; linelength < 15; ++linelength) {
		close(fds[linelength]);
	}
	return 0;

fail_badread:
fail_queues_commit:
fail_queue_arr_push:
fail_baddate:
fail_get_header:
fail_parse_date:
fail_get_date:
	readline_free(&rlData);
fail_readline_init:
	for (linelength = 0; linelength < 15; ++linelength) {
		queue_free(&(queues[linelength]));
	}
fail_queue_arr_init:
	for (linelength = 0; linelength < 15; ++linelength) {
		close(fds[linelength]);
	}
fail_fd_arr_init:
	return 1;
}
