#include "readline.h" /* read(), malloc(), free() */

int
readline_init(struct rl_data *data, int fd, void *buf, size_t bufcap)
{
	assert(data != NULL);
	assert(fd >= 0);
	assert(bufcap > 0);

	if (buf == NULL) { /* Allow user to specify their own buffer. */
		data->allocated = 1;
		buf = malloc(bufcap);
	} else {
		data->allocated = 0;
	}
	if (buf == NULL) {
		return 1;
	}
	data->buf = (char *)buf;
	data->bptr = data->buf;
	data->bcap = bufcap;
	data->fd = fd;
	data->blen = 0;
	data->ec = 0;

	assert(data->buf != NULL);
	assert(data->fd >= 0);
	assert(data->bcap > 0);

	return 0;
}

void
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

ssize_t
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
			/*0 is a valid return size, so shift error codes down*/
			if (data->blen == 0 || data->blen == -1) {
				data->ec = data->blen;
				return -1;
			}
		}
		if (*data->bptr == '\n') {
			*bufptr++ = *data->bptr++;
			break;
		}
		*bufptr++ = *data->bptr++;
	}
	*bufptr = '\0';
	return (bufptr - buf);
}

int
readline_err(struct rl_data *data)
{
	return data->ec;
}
