#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum StatementType {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct QualifiedName {
    char *schema;
    char *table;
} QualifiedName;

typedef struct InsertStatement {
    QualifiedName target;
    bool has_column_list;
    size_t column_count;
    char **columns;
    size_t value_count;
    char **values;
} InsertStatement;

typedef struct WhereClause {
    char *column;
    char *value;
} WhereClause;

typedef struct SelectStatement {
    QualifiedName source;
    bool select_all;
    size_t column_count;
    char **columns;
    bool has_where_clause;
    WhereClause where_clause;
} SelectStatement;

typedef struct Statement {
    StatementType type;
    union {
        InsertStatement insert_statement;
        SelectStatement select_statement;
    } as;
} Statement;

typedef struct StatementList {
    Statement *items;
    size_t count;
} StatementList;

bool parse_sql_script(const char *sql,
                      StatementList *out_list,
                      char *error_buffer,
                      size_t error_buffer_size);
void free_statement_list(StatementList *list);

#endif
