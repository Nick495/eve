#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL */
#include <stdint.h> /* uint*_t */
#include <ctype.h>  /* isdigit() */
#include "eve_txn.h" /* struct eve_txn */

typedef int (*eve_txn_parser)(const char *str, struct eve_txn *rec);

eve_txn_parser eve_txn_parser_strategy(uint32_t yr, uint32_t mn, uint32_t dy);

#endif
