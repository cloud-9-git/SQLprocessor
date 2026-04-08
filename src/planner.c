#include "sqlproc/planner.h"

#include <stdlib.h>
#include <string.h>

static char *dup_string(const char *text, SqlError *err) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

static void plan_node_free(PlanNode *node) {
    if (node == NULL) {
        return;
    }

    plan_node_free(node->child);
    switch (node->kind) {
        case PLAN_NODE_INSERT:
            free(node->as.insert.table_name);
            row_free(&node->as.insert.row);
            break;
        case PLAN_NODE_SEQ_SCAN:
            free(node->as.seq_scan.table_name);
            break;
        case PLAN_NODE_FILTER:
            value_free(&node->as.filter.value);
            break;
        case PLAN_NODE_PROJECT:
            free(node->as.project.projection_indices);
            break;
        default:
            break;
    }
    free(node);
}

static PlanNode *new_node(PlanNodeKind kind, SqlError *err) {
    PlanNode *node = (PlanNode *)calloc(1U, sizeof(PlanNode));

    if (node == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }
    node->kind = kind;
    return node;
}

void plan_script_init(PlanScript *script) {
    if (script == NULL) {
        return;
    }

    script->statement_count = 0U;
    script->statements = NULL;
}

void plan_script_free(PlanScript *script) {
    size_t index;

    if (script == NULL) {
        return;
    }

    for (index = 0U; index < script->statement_count; index++) {
        table_schema_free(&script->statements[index].schema);
        plan_node_free(script->statements[index].root);
    }

    free(script->statements);
    script->statements = NULL;
    script->statement_count = 0U;
}

static SqlStatus clone_projection_indices(const size_t *src, size_t count, size_t **out_indices, SqlError *err) {
    size_t *copy;

    *out_indices = NULL;
    if (count == 0U) {
        return SQL_STATUS_OK;
    }

    copy = (size_t *)malloc(sizeof(size_t) * count);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    memcpy(copy, src, sizeof(size_t) * count);
    *out_indices = copy;
    return SQL_STATUS_OK;
}

static SqlStatus build_insert_plan(const BoundInsertStmt *insert, PlanStatement *out_statement, SqlError *err) {
    PlanNode *node = new_node(PLAN_NODE_INSERT, err);
    SqlStatus status;

    if (node == NULL) {
        return SQL_STATUS_OOM;
    }

    node->as.insert.table_name = dup_string(insert->schema.table_name, err);
    if (insert->schema.table_name != NULL && node->as.insert.table_name == NULL) {
        plan_node_free(node);
        return SQL_STATUS_OOM;
    }

    row_init(&node->as.insert.row);
    status = row_clone(&insert->row, &node->as.insert.row, err);
    if (status != SQL_STATUS_OK) {
        plan_node_free(node);
        return status;
    }

    out_statement->root = node;
    return SQL_STATUS_OK;
}

static SqlStatus build_select_plan(const BoundSelectStmt *select, PlanStatement *out_statement, SqlError *err) {
    PlanNode *scan = new_node(PLAN_NODE_SEQ_SCAN, err);
    PlanNode *current;
    PlanNode *filter = NULL;
    PlanNode *project = NULL;
    SqlStatus status;

    if (scan == NULL) {
        return SQL_STATUS_OOM;
    }

    scan->as.seq_scan.table_name = dup_string(select->schema.table_name, err);
    if (select->schema.table_name != NULL && scan->as.seq_scan.table_name == NULL) {
        plan_node_free(scan);
        return SQL_STATUS_OOM;
    }

    current = scan;
    if (select->has_filter) {
        filter = new_node(PLAN_NODE_FILTER, err);
        if (filter == NULL) {
            plan_node_free(scan);
            return SQL_STATUS_OOM;
        }
        filter->child = current;
        filter->as.filter.column_index = select->filter.column_index;
        status = value_clone(&select->filter.value, &filter->as.filter.value, err);
        if (status != SQL_STATUS_OK) {
            plan_node_free(filter);
            return status;
        }
        current = filter;
    }

    project = new_node(PLAN_NODE_PROJECT, err);
    if (project == NULL) {
        plan_node_free(current);
        return SQL_STATUS_OOM;
    }

    project->child = current;
    project->as.project.projection_count = select->projection_count;
    status = clone_projection_indices(select->projection_indices,
                                      select->projection_count,
                                      &project->as.project.projection_indices,
                                      err);
    if (status != SQL_STATUS_OK) {
        plan_node_free(project);
        return status;
    }

    out_statement->root = project;
    return SQL_STATUS_OK;
}

SqlStatus planner_build_script(const BoundScript *bound, PlanScript *out_plan, SqlError *err) {
    size_t index;

    if (bound == NULL || out_plan == NULL) {
        sql_error_set(err, 0, 0, 0, "planner_build_script received null pointer");
        return SQL_STATUS_ERROR;
    }

    plan_script_init(out_plan);
    if (bound->statement_count == 0U) {
        return SQL_STATUS_OK;
    }

    out_plan->statements = (PlanStatement *)calloc(bound->statement_count, sizeof(PlanStatement));
    if (out_plan->statements == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    out_plan->statement_count = bound->statement_count;
    for (index = 0U; index < bound->statement_count; index++) {
        const BoundStatement *source = &bound->statements[index];
        PlanStatement *target = &out_plan->statements[index];
        SqlStatus status;

        table_schema_init(&target->schema);
        target->root = NULL;
        target->line = source->line;
        target->column = source->column;
        target->kind = (source->kind == BOUND_STATEMENT_INSERT) ? PLAN_STATEMENT_INSERT : PLAN_STATEMENT_SELECT;

        status = table_schema_clone(source->kind == BOUND_STATEMENT_INSERT
                                        ? &source->as.insert_stmt.schema
                                        : &source->as.select_stmt.schema,
                                    &target->schema,
                                    err);
        if (status != SQL_STATUS_OK) {
            plan_script_free(out_plan);
            return status;
        }

        if (source->kind == BOUND_STATEMENT_INSERT) {
            status = build_insert_plan(&source->as.insert_stmt, target, err);
        } else {
            status = build_select_plan(&source->as.select_stmt, target, err);
        }

        if (status != SQL_STATUS_OK) {
            plan_script_free(out_plan);
            return status;
        }
    }

    return SQL_STATUS_OK;
}

const char *plan_node_kind_name(PlanNodeKind kind) {
    switch (kind) {
        case PLAN_NODE_INSERT:
            return "INSERT";
        case PLAN_NODE_SEQ_SCAN:
            return "SEQ_SCAN";
        case PLAN_NODE_FILTER:
            return "FILTER";
        case PLAN_NODE_PROJECT:
            return "PROJECT";
        case PLAN_NODE_INDEX_SCAN:
            return "INDEX_SCAN";
        case PLAN_NODE_LIMIT:
            return "LIMIT";
        case PLAN_NODE_SORT:
            return "SORT";
        default:
            return "UNKNOWN";
    }
}
