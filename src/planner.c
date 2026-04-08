#include "sqlproc/planner.h"

#include <stdlib.h>
#include <string.h>

/* plan 안에 저장할 문자열을 새 메모리로 복사합니다. */
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

/* plan tree를 재귀적으로 해제합니다. */
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

/* 새 PlanNode를 0으로 초기화해서 생성합니다. */
static PlanNode *new_node(PlanNodeKind kind, SqlError *err) {
    PlanNode *node = (PlanNode *)calloc(1U, sizeof(PlanNode));

    if (node == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }
    node->kind = kind;
    return node;
}

/* PlanScript 컨테이너를 빈 상태로 초기화합니다. */
void plan_script_init(PlanScript *script) {
    if (script == NULL) {
        return;
    }

    script->statement_count = 0U;
    script->statements = NULL;
}

/* PlanScript 전체를 순회하며 schema와 plan tree를 해제합니다. */
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

/* projection index 배열을 plan 전용 메모리로 복사합니다. */
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

/* BoundInsertStmt를 INSERT plan node 하나로 감쌉니다. */
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

/* BoundSelectStmt를 SELECT용 논리 계획 체인으로 변환합니다. */
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

    /* ⭐ v1의 SELECT 계획은 아래처럼 단일 체인으로 구성됩니다.
     * PROJECT -> FILTER -> SEQ_SCAN
     */
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

/* 🧭 planner의 진입점입니다.
 * binder가 해석한 의미를 "어떤 순서로 실행할지"라는 계획 구조로 바꿉니다.
 */
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
    /* planner는 실행 모양만 결정합니다.
     * SQL 문법 해석과 스키마 검증은 앞선 단계에서 끝났다고 가정합니다.
     */
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

/* trace 출력용으로 PlanNodeKind를 문자열로 바꿉니다. */
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
