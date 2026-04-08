#ifndef SQLPROC_PARSER_H
#define SQLPROC_PARSER_H

#include "sqlproc/ast.h"
#include "sqlproc/diag.h"

/* 🧭 SQL 문자열을 AST 스크립트로 바꾸는 진입 함수입니다. */
SqlStatus parser_parse_script(const char *sql, AstScript *out_script, SqlError *err);

#endif
