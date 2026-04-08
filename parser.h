#ifndef PARSER_H
#define PARSER_H

#include "types.h"

/* ?섎굹??SQL 臾몄옄?댁쓣 ?뚯떛??Statement濡?蹂?섑빀?덈떎. */
/* ?깃났?섎㈃ true(1), ?ㅽ뙣?섎㈃ false(0) 諛섑솚. */
int parse_statement(const char *sql, Statement *stmt);

#endif
