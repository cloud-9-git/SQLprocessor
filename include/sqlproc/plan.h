#ifndef SQLPROC_PLAN_H
#define SQLPROC_PLAN_H

#include <stddef.h>

#include "sqlproc/schema.h"

typedef enum {
    PLAN_NODE_INSERT = 0,
    PLAN_NODE_SEQ_SCAN = 1,
    PLAN_NODE_FILTER = 2,
    PLAN_NODE_PROJECT = 3,
    PLAN_NODE_INDEX_SCAN = 4,
    PLAN_NODE_LIMIT = 5,
    PLAN_NODE_SORT = 6
} PlanNodeKind;

typedef struct PlanNode PlanNode;

struct PlanNode {
    PlanNodeKind kind;
    PlanNode *child;
    union {
        struct {
            char *table_name;
            Row row;
        } insert;
        struct {
            char *table_name;
        } seq_scan;
        struct {
            size_t column_index;
            Value value;
        } filter;
        struct {
            size_t projection_count;
            size_t *projection_indices;
        } project;
    } as;
};

typedef enum {
    PLAN_STATEMENT_INSERT = 0,
    PLAN_STATEMENT_SELECT = 1
} PlanStatementKind;

typedef struct {
    PlanStatementKind kind;
    int line;
    int column;
    TableSchema schema;
    PlanNode *root;
} PlanStatement;

typedef struct {
    size_t statement_count;
    PlanStatement *statements;
} PlanScript;

void plan_script_init(PlanScript *script);
void plan_script_free(PlanScript *script);
const char *plan_node_kind_name(PlanNodeKind kind);

#endif
