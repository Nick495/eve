#include "queue.h"

#define HEADERSIZE 2

int
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

void
queue_free(struct queue *q)
{
	free(q->data);
	free(q->page);
	return;
}

int
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

int
queue_compress(struct queue *q)
{
	const unsigned int lower_limit = 11; /* Comes from lz4, refactor. */
	const unsigned int buffer_len = (q->dUse * q->eleSize);
	int uc_bytes = buffer_len; /* Try to compress all buffer's bytes. */
	uint16_t new_elements = 0;

	/* Bytes taken in page by LZ4 compression. */
	int c_bytes = LZ4_compress_destSize(q->data, q->page + q->pUse,
		&uc_bytes, q->pSize - q->pUse);
	if (c_bytes == 0) {
		return -1;
	}

	if (uc_bytes < q->eleSize) {
		/* Below, we try to estimate how much space we have to leave in the
		 * buffer to ensure that we can keep compressing stuff. Sometimes
		 * our estimate will be wrong, so we have to handle that case. */
		queue_write(q);
		return queue_compress(q);
	}

	/* We don't want our integers cut on page boundaries, So we tell lz4
	 * that our buffer is only as long as the number of integers it can
	 * successfully compress. */
	if (uc_bytes % q->eleSize != 0) {
		uc_bytes -= uc_bytes % q->eleSize;
		c_bytes = LZ4_compress_destSize(q->data, q->page + q->pUse,
			&uc_bytes, q->pSize - q->pUse);
		if (c_bytes == 0) {
			return -1;
		}
	}

	new_elements = (uint16_t)(uc_bytes / q->eleSize);
	q->pEleCount += new_elements;
	q->dUse -= new_elements;
	q->pUse += c_bytes;

	if (q->pSize - q->pUse <= lower_limit) {
		queue_write(q);
	}

	memmove(q->data, (char *)q->data + uc_bytes, buffer_len - uc_bytes);
	return 0;
}

int
queue_push(struct queue *q, const void *data)
{
	if (q->dUse < q->dCap) {
		memcpy((char *)q->data + q->dUse * q->eleSize, data, q->eleSize);
		q->dUse++;
		return 0;
	}
	if (queue_compress(q)) {
		return 1;
	}
	return queue_push(q, data);
}

int
queue_commit(struct queue *q)
{
	if (q->dUse == 0 && q->pUse == 0) { /* All data has been flushed. */
		return 0;
	}
	if (q->dUse == 0) { /* All data is in the compressed buffer. */
		return queue_write(q);
	}
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
