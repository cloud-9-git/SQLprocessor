#ifndef SQLPROC_PARSER_H
#define SQLPROC_PARSER_H

#include "sqlproc/ast.h"
#include "sqlproc/diag.h"

SqlStatus parser_parse_script(const char *sql, AstScript *out_script, SqlError *err);

#endif
