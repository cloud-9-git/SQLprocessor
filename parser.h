#ifndef PARSER_H
#define PARSER_H

#include "types.h"

// 하나의 SQL 문자열을 파싱해 Statement로 변환합니다.
// 성공하면 true(1), 실패하면 false(0) 반환.
int parse_statement(const char *sql, Statement *stmt);

#endif
