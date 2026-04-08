#include "sqlproc/executor.h"

#include "sqlproc/storage.h"

#include <stdlib.h>
#include <string.h>

/* storage 콜백 실행 중 현재 statement와 결과셋을 함께 넘기기 위한 컨텍스트입니다. */
typedef struct {
    const PlanStatement *statement;
    ResultSet *result;
} ScanContext;

/* ResultSet 하나가 들고 있는 컬럼 이름과 row 메모리를 해제합니다. */
static void result_set_free(ResultSet *result) {
    size_t index;

    if (result == NULL) {
        return;
    }

    for (index = 0U; index < result->column_count; index++) {
        free(result->column_names[index]);
    }
    free(result->column_names);
    result->column_names = NULL;

    for (index = 0U; index < result->row_count; index++) {
        row_free(&result->rows[index]);
    }
    free(result->rows);
    result->rows = NULL;
    result->row_count = 0U;
    result->column_count = 0U;
}

/* ExecutionOutput을 빈 상태로 초기화합니다. */
void execution_output_init(ExecutionOutput *output) {
    if (output == NULL) {
        return;
    }

    output->result_count = 0U;
    output->results = NULL;
}

/* ExecutionOutput에 들어 있는 모든 ResultSet을 해제합니다. */
void execution_output_free(ExecutionOutput *output) {
    size_t index;

    if (output == NULL) {
        return;
    }

    for (index = 0U; index < output->result_count; index++) {
        result_set_free(&output->results[index]);
    }
    free(output->results);
    output->results = NULL;
    output->result_count = 0U;
}

/* 결과셋에 저장할 컬럼 이름을 복사할 때 쓰는 헬퍼입니다. */
static char *dup_string(const char *text, SqlError *err) {
    size_t length;
    char *copy;

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* 단일 체인 plan에서 원하는 종류의 노드를 찾아 반환합니다. */
static const PlanNode *find_node(const PlanNode *root, PlanNodeKind kind) {
    const PlanNode *current = root;

    while (current != NULL) {
        if (current->kind == kind) {
            return current;
        }
        current = current->child;
    }

    return NULL;
}

/* 원본 row에서 projection에 해당하는 컬럼만 뽑아 새 row를 만듭니다. */
static SqlStatus clone_projected_row(const Row *source,
                                     const size_t *projection_indices,
                                     size_t projection_count,
                                     Row *out_row,
                                     SqlError *err) {
    size_t index;

    row_init(out_row);
    out_row->value_count = projection_count;
    out_row->values = (Value *)calloc(projection_count, sizeof(Value));
    if (projection_count > 0U && out_row->values == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    for (index = 0U; index < projection_count; index++) {
        SqlStatus status = value_clone(&source->values[projection_indices[index]], &out_row->values[index], err);
        if (status != SQL_STATUS_OK) {
            row_free(out_row);
            return status;
        }
    }

    return SQL_STATUS_OK;
}

/* storage가 읽어온 row를 현재 SELECT 계획에 맞게 filter/project 해서 결과셋에 추가합니다. */
static SqlStatus scan_callback(const Row *row, void *ctx, SqlError *err) {
    ScanContext *scan_ctx = (ScanContext *)ctx;
    const PlanNode *filter = find_node(scan_ctx->statement->root, PLAN_NODE_FILTER);
    const PlanNode *project = find_node(scan_ctx->statement->root, PLAN_NODE_PROJECT);
    ResultSet *result = scan_ctx->result;
    Row *grown_rows;
    SqlStatus status;

    /* ⭐ 스캔된 각 row는 항상 `FILTER -> PROJECT` 순서로 처리됩니다. */
    if (filter != NULL) {
        if (!value_equal(&row->values[filter->as.filter.column_index], &filter->as.filter.value)) {
            return SQL_STATUS_OK;
        }
    }

    grown_rows = (Row *)realloc(result->rows, sizeof(Row) * (result->row_count + 1U));
    if (grown_rows == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }
    result->rows = grown_rows;

    status = clone_projected_row(row,
                                 project->as.project.projection_indices,
                                 project->as.project.projection_count,
                                 &result->rows[result->row_count],
                                 err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    result->row_count++;
    return SQL_STATUS_OK;
}

/* SELECT statement 하나를 실제 결과셋으로 실행합니다. */
static SqlStatus execute_select(ExecutionContext *ctx,
                                const PlanStatement *statement,
                                ResultSet *out_result,
                                SqlError *err) {
    const PlanNode *project = find_node(statement->root, PLAN_NODE_PROJECT);
    const PlanNode *scan = find_node(statement->root, PLAN_NODE_SEQ_SCAN);
    size_t index;
    ScanContext scan_ctx;

    if (project == NULL || scan == NULL) {
        sql_error_set(err, statement->line, statement->column, 0, "malformed SELECT plan");
        return SQL_STATUS_ERROR;
    }

    /* 출력 컬럼 순서는 storage 순서가 아니라 PROJECT 노드가 결정합니다. */
    out_result->column_count = project->as.project.projection_count;
    out_result->column_names = (char **)calloc(out_result->column_count, sizeof(char *));
    out_result->row_count = 0U;
    out_result->rows = NULL;
    if (out_result->column_count > 0U && out_result->column_names == NULL) {
        sql_error_set(err, statement->line, statement->column, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    for (index = 0U; index < out_result->column_count; index++) {
        size_t schema_index = project->as.project.projection_indices[index];
        out_result->column_names[index] = dup_string(statement->schema.columns[schema_index].name, err);
        if (out_result->column_names[index] == NULL) {
            result_set_free(out_result);
            return SQL_STATUS_OOM;
        }
    }

    scan_ctx.statement = statement;
    scan_ctx.result = out_result;
    return storage_scan_rows(ctx->db_root, &statement->schema, scan_callback, &scan_ctx, err);
}

/* 🧭 executor의 진입점입니다.
 * INSERT는 저장, SELECT는 ResultSet 생성으로 분기해 statement를 순차 실행합니다.
 */
SqlStatus executor_run_script(ExecutionContext *ctx, const PlanScript *plan, ExecutionOutput *out_output, SqlError *err) {
    size_t index;

    if (ctx == NULL || plan == NULL || out_output == NULL) {
        sql_error_set(err, 0, 0, 0, "executor_run_script received null pointer");
        return SQL_STATUS_ERROR;
    }

    execution_output_init(out_output);
    /* ⚠️ statement-level autocommit 방식입니다.
     * 앞 문장이 성공한 뒤 뒤 문장이 실패해도 앞 문장 결과는 유지됩니다.
     */
    for (index = 0U; index < plan->statement_count; index++) {
        const PlanStatement *statement = &plan->statements[index];
        SqlStatus status;

        if (statement->kind == PLAN_STATEMENT_INSERT) {
            status = storage_append_row(ctx->db_root,
                                        &statement->schema,
                                        &statement->root->as.insert.row,
                                        err);
            if (status != SQL_STATUS_OK) {
                if (err != NULL) {
                    err->statement_index = (int)(index + 1U);
                    if (err->line == 0) {
                        err->line = statement->line;
                        err->column = statement->column;
                    }
                }
                execution_output_free(out_output);
                return status;
            }
        } else if (statement->kind == PLAN_STATEMENT_SELECT) {
            ResultSet *grown_results = (ResultSet *)realloc(out_output->results,
                                                            sizeof(ResultSet) * (out_output->result_count + 1U));
            if (grown_results == NULL) {
                sql_error_set(err, statement->line, statement->column, (int)(index + 1U), "out of memory");
                execution_output_free(out_output);
                return SQL_STATUS_OOM;
            }

            out_output->results = grown_results;
            memset(&out_output->results[out_output->result_count], 0, sizeof(ResultSet));
            status = execute_select(ctx, statement, &out_output->results[out_output->result_count], err);
            if (status != SQL_STATUS_OK) {
                if (err != NULL) {
                    err->statement_index = (int)(index + 1U);
                    if (err->line == 0) {
                        err->line = statement->line;
                        err->column = statement->column;
                    }
                }
                execution_output_free(out_output);
                return status;
            }

            out_output->result_count++;
        }
    }

    return SQL_STATUS_OK;
}
