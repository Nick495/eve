/**
   @file      mkNDBM.c
   @author    Mitch Richling <http://www.mitchr.me/>
   @Copyright Copyright 1997 by Mitch Richling.  All rights reserved.
   @brief     How to create and populate an NDBM DB@EOL
   @Keywords  UNIX database dbm ndbm gdbm
   @Std       ISOC POSIX UNIX98 BSD4.3 SYSV3

   @Tested
              - Solaris 2.8
              - MacOS X.2
*/

#include <fcntl.h>              /* UNIX file ctrl  UNIX  */
#include <string.h>             /* Strings         ISOC  */
#include <stdlib.h>             /* Standard Lib    ISOC  */
#include <stdio.h>              /* I/O lib         ISOC  */
#include <unistd.h>             /* UNIX std stf    POSIX */
#include <errno.h>              /* error stf       POSIX */
#include "dbm.h"                /* ndbm header     BSD   */

#define NUMELE 1000000
#define BUFLEN 102 /* Good for a NUMELE < 10 sexdecillion */
#define ALPLEN 26

size_t strgen(char *str, size_t val, const char offset);

int main(void)
{
	int dbRet;
	datum daKey, daVal;
	char key[BUFLEN];
	char val[BUFLEN];
	size_t i;

	/* Open the database (create) */
	dbRet = dbminit("./dbmTest");
	if (dbRet) {
		printf("ERROR: Could not open the DB file. "
			"Error number: %d.\n", errno);
		exit(1);
	} else {
		printf("DB created.\n");
		printf("DB opened.\n");
	}

	/* Slight optimization */
	daKey.dptr = key;
	daVal.dptr = val;

	/* Store. */
	for (i = 0; i < NUMELE; ++i) {
		daKey.dsize = strgen(key, i, 'a');
		daVal.dsize = strgen(val, i*2, 'a');
		store(daKey, daVal);
		printf("Store: '%s' ==> '%s'\n",
			(char *)daKey.dptr, (char *)daVal.dptr);
	}

	/* Lookup */
	for (i = 0; i < NUMELE; ++i) {
		daKey.dsize = strgen(key, i, 'a');
		daVal = fetch(daKey);
		if (daVal.dptr == NULL) {
			printf("ERROR: Could not look up %s\n",
				(char *)daKey.dptr);
		} else {
			printf("Got record: '%s' ==> '%s'\n",
				(char *)daKey.dptr, (char *)daVal.dptr);
		}
	}

	/* Close the DB (flush everything to the file) */
	printf("DB closed... Bye!\n");

	return 0;
}

void strrev(char *str, size_t len)
{
	int c;
	size_t index;

	for (index = 0, len = len - 1; index < len; index++, len--) {
		c = str[index];
		str[index] = str[len];
		str[len] = c;
	}

	return;
}

size_t strgen(char *str, size_t val, const char offset)
{
	size_t index = 0;
	do {
		str[index++] = val % ALPLEN + offset;
		val /= ALPLEN;
	} while (val != 0);
	str[index] = '\0';
	strrev(str, index);

	return index;
}
