#include "tss.h"

static int errhandle_openfd(const char *path, const int flags)
{
	int rc;
	int err;
	errno = 0;
	while ((rc = open(path, flags)) == -1) {
		switch(err = errno) {
		case EINTR:
			continue;
		case EACCES:
		case EDQUOT:
		case EISDIR:
		case ENOENT:
		case ENOSPC:
		case ENOTDIR:
		case EROFS:
		case EBADF:
		case ENOTDIR:
			return err;
		default:
			assert(0);
		}
	}

	return rc;
}

int extcmp(const char *a, const size_t alen, const char *e, const size_t elen)
{
	int i;
	if (alen <= elen) {
		return 1;
	}

	for (i = 0; i < elen; ++i) {
		if (a[alen - elen + i] != e[i]) {
			return 1;
		}
	}

	return 0;
}

static int dir_search_ext(const char *dpath, const char *ext)
{
	struct DIR *dirp = NULL;
	struct DIR *dp = NULL;
	char buf[4096];
	const size_t extlen = strlen(ext);
	assert(ext[extlen] == '\0');

	dirp = opendir(dpath); /* Uses malloc() behind the scenes. */
	if (dirp == NULL) {
		return -1;
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (extcmp(dp->d_name, dp->d_namelen, ext, extlen)) {
			continue;
		}

		memcpy(buf, dp->d_name, dp->d_namelen + 1);
		
	}

}

TSS_ERROR tss_open(TSS **tss, char *rpath)
{
	int rc, walfd;
	char *buf;

	const size_t rplen = strlen(rpath);
	assert(rpath[rplen] == '\0');

	buf = malloc(rplen + 256);
	memcpy(buf, rpath, rplen);
	memcpy(buf + rplen, "wal", strlen("wal") + 1); /* null terminator */

	/* Search for the WAL file */
	if ((rc = errhandle_open(buf, O_RDONLY|O_APPEND)) < 0) {
		return F_BADVAL;
	}
	walfd = rc;

}
