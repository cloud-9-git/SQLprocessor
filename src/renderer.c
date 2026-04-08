#include "sqlproc/renderer.h"

#include <stdio.h>
#include <stdlib.h>

static void print_indent(FILE *stream, int depth) {
    for (int index = 0; index < depth; index++) {
        fputs("  ", stream);
    }
}

static void print_plan_node(FILE *stream, const PlanNode *node, int depth) {
    if (node == NULL) {
        return;
    }

    print_indent(stream, depth);
    fprintf(stream, "- %s", plan_node_kind_name(node->kind));
    if (node->kind == PLAN_NODE_SEQ_SCAN) {
        fprintf(stream, " table=%s", node->as.seq_scan.table_name);
    } else if (node->kind == PLAN_NODE_FILTER) {
        fprintf(stream, " column_index=%zu", node->as.filter.column_index);
    } else if (node->kind == PLAN_NODE_PROJECT) {
        fprintf(stream, " columns=%zu", node->as.project.projection_count);
    } else if (node->kind == PLAN_NODE_INSERT) {
        fprintf(stream, " table=%s", node->as.insert.table_name);
    }
    fputc('\n', stream);
    print_plan_node(stream, node->child, depth + 1);
}

SqlStatus renderer_print_results(FILE *stream, const ExecutionOutput *output, SqlError *err) {
    size_t result_index;

    if (stream == NULL || output == NULL) {
        sql_error_set(err, 0, 0, 0, "renderer_print_results received null pointer");
        return SQL_STATUS_ERROR;
    }

    for (result_index = 0U; result_index < output->result_count; result_index++) {
        const ResultSet *result = &output->results[result_index];

        for (size_t column = 0U; column < result->column_count; column++) {
            if (column > 0U) {
                fputc('\t', stream);
            }
            fputs(result->column_names[column], stream);
        }
        fputc('\n', stream);

        for (size_t row_index = 0U; row_index < result->row_count; row_index++) {
            const Row *row = &result->rows[row_index];
            for (size_t column = 0U; column < row->value_count; column++) {
                char *plain = NULL;
                SqlStatus status;

                if (column > 0U) {
                    fputc('\t', stream);
                }

                status = value_to_plain_text(&row->values[column], &plain, err);
                if (status != SQL_STATUS_OK) {
                    return status;
                }
                fputs(plain, stream);
                free(plain);
            }
            fputc('\n', stream);
        }

        if (result_index + 1U < output->result_count) {
            fputc('\n', stream);
        }
    }

    return SQL_STATUS_OK;
}

void renderer_print_error(FILE *stream, const SqlError *err) {
    if (stream == NULL || err == NULL) {
        return;
    }

    fprintf(stream,
            "error [statement %d, line %d, column %d]: %s\n",
            err->statement_index,
            err->line,
            err->column,
            err->message);
}

void renderer_print_trace(FILE *stream, const PlanScript *plan) {
    if (stream == NULL || plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->statement_count; index++) {
        const PlanStatement *statement = &plan->statements[index];
        fprintf(stream,
                "statement %zu: %s (line=%d, column=%d)\n",
                index + 1U,
                statement->kind == PLAN_STATEMENT_INSERT ? "INSERT" : "SELECT",
                statement->line,
                statement->column);
        print_plan_node(stream, statement->root, 1);
    }
}

void renderer_print_check_ok(FILE *stream, size_t statement_count) {
    if (stream == NULL) {
        return;
    }

    fprintf(stream, "validation OK: %zu statement(s)\n", statement_count);
}
