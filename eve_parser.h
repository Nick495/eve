#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h> /* assert() */

#include "token.h"

typedef int (*Parser)(token *tokens, const size_t tokenCount);

Parser parser_factory(const uint32_t yr, const uint32_t mn, const uint32_t dy);

#endif /* EVE_LOADER_H_ */
