#ifndef DARRAY_XMACRO_H__
#define DARRAY_XMACRO_H__

/* E_ and ERROR_ are reserved by POSIX> */
#define sequence \
    select(SUCCESS, "No error.")\
    select(EMPTY_FILE, "The file is empty, no persistant data. No error.")\
    select(F_WRAP, "Unsigned integer wrap.")\
    select(F_STAT, "Failed to stat() given file.")\
    select(F_OPEN, "Failed to open() given file. (NOTE: permissions rw)")\
    select(F_LSEEK, "Failed to lseek() given file's given location.")\
    select(F_WRITE, "Failed to write() to the given file.")\
    select(F_CLOSE, "Failed to close() the given file.")\
    select(F_MMAP, "Failed to mmap() the given file. NOTE: permissions rw")\
    select(F_MSYNC, "Failed to msync() the given file.")\
    select(F_MUNMAP, "Failed to munmap() the given file.")\
    select(F_BAD_IDENTIFIER, "The given file is corrupt or invalid.")\
    select(F_BAD_VERSION, "Cannot read given file's version.")\
    select(F_BAD_TYPE, "The given file is either corrupted or invalid.")\
    select(F_BAD_USECAP, "The given file is either corrupted or invalid.")\
    select(F_MISMATCH_TYPE, "The file's TYPE conflicts with that given.")\
    select(F_MISMATCH_VALUE_SIZE, \
            "The file's val_size conflicts with that given.")\
    select(F_BADVAL, "A bad parameter was provided.")

// Generate the enum.
#undef select
#define select(symbol, string) symbol,
enum TSS_ERROR { sequence };

#undef select
#define select(symbol, string) #string,
static const char * errorStrings[] = { sequence };

const char *tss_strerror(int err) {
	return errorStrings[err];
}

#endif
