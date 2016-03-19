#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h> /* assert() */
#include <unistd.h> /* STDIN_FILENO */
#include <ctype.h>  /* isdigit() */

#include "token.h"

typedef int (*Parser)(const char *str, token *tokens, const size_t tokenCount);

Parser parser_factory(const uint32_t yr, const uint32_t mn, const uint32_t dy);
Parser parser_factory2(const uint32_t yr, const uint32_t mn, const uint32_t dy);

#endif /* EVE_LOADER_H_ */
