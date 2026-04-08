#ifndef LEXER_H
#define LEXER_H

#include "types.h"

// SQL 문자열을 읽기 시작할 수 있게 lexer 초기 상태를 준비합니다.
void init_lexer(Lexer *l, const char *sql);

// 현재 위치에서 한 토큰을 추출해 반환합니다.
// 토큰 종류/문자열을 채워 파서가 바로 사용할 수 있게 만듭니다.
Token get_next_token(Lexer *l);

#endif
