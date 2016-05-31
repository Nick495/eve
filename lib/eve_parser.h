#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL */
#include <stdint.h> /* uint*_t */
#include <ctype.h>  /* isdigit() */
#include "eve_txn.h" /* struct eve_txn */

typedef int (*eve_txn_parser)(const char *str, struct eve_txn *rec);

eve_txn_parser init_eve_txn_parser(uint32_t year, uint32_t mon, uint32_t day);

#endif
