#include "sqlproc/binder.h"

#include <stdlib.h>

static SqlStatus clone_schema_from_catalog(const Catalog *catalog,
                                           const char *table_name,
                                           TableSchema *out_schema,
                                           int line,
                                           int column,
                                           int statement_index,
                                           SqlError *err) {
    SqlStatus status = catalog_load_table(catalog->db_root, table_name, out_schema, err);

    if (status != SQL_STATUS_OK && err != NULL && err->line == 0 && err->column == 0) {
        err->line = line;
        err->column = column;
        err->statement_index = statement_index;
    } else if (err != NULL) {
        err->statement_index = statement_index;
    }

    return status;
}

static SqlStatus ensure_type_match(DataType expected,
                                   const Value *actual,
                                   int line,
                                   int column,
                                   int statement_index,
                                   SqlError *err) {
    if (actual->type != expected) {
        sql_error_set(err, line, column, statement_index,
                      "type mismatch: expected %s but found %s",
                      value_type_name(expected), value_type_name(actual->type));
        return SQL_STATUS_ERROR;
    }

    return SQL_STATUS_OK;
}

static SqlStatus bind_insert(const Statement *statement,
                             const TableSchema *schema,
                             BoundInsertStmt *out_insert,
                             int statement_index,
                             SqlError *err) {
    const InsertStmt *insert = &statement->as.insert_stmt;
    size_t index;
    SqlStatus status;

    row_init(&out_insert->row);
    if (insert->value_count == 0U) {
        sql_error_set(err, statement->line, statement->column, statement_index, "INSERT must provide at least one value");
        return SQL_STATUS_ERROR;
    }

    status = table_schema_clone(schema, &out_insert->schema, err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    out_insert->row.value_count = schema->column_count;
    out_insert->row.values = (Value *)calloc(schema->column_count, sizeof(Value));
    if (out_insert->row.values == NULL) {
        table_schema_free(&out_insert->schema);
        sql_error_set(err, statement->line, statement->column, statement_index, "out of memory");
        return SQL_STATUS_OOM;
    }

    if (insert->column_count == 0U) {
        if (insert->value_count != schema->column_count) {
            table_schema_free(&out_insert->schema);
            row_free(&out_insert->row);
            sql_error_set(err, statement->line, statement->column, statement_index,
                          "INSERT value count (%zu) does not match schema column count (%zu)",
                          insert->value_count, schema->column_count);
            return SQL_STATUS_ERROR;
        }

        for (index = 0U; index < schema->column_count; index++) {
            status = ensure_type_match(schema->columns[index].type,
                                       &insert->values[index],
                                       statement->line,
                                       statement->column,
                                       statement_index,
                                       err);
            if (status != SQL_STATUS_OK) {
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                return status;
            }

            status = value_clone(&insert->values[index], &out_insert->row.values[index], err);
            if (status != SQL_STATUS_OK) {
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                return status;
            }
        }

        return SQL_STATUS_OK;
    }

    if (insert->column_count != schema->column_count || insert->value_count != schema->column_count) {
        table_schema_free(&out_insert->schema);
        row_free(&out_insert->row);
        sql_error_set(err, statement->line, statement->column, statement_index,
                      "partial INSERT is not supported; provide all %zu columns explicitly",
                      schema->column_count);
        return SQL_STATUS_ERROR;
    }

    {
        int *assigned = (int *)calloc(schema->column_count, sizeof(int));
        if (assigned == NULL) {
            table_schema_free(&out_insert->schema);
            row_free(&out_insert->row);
            sql_error_set(err, statement->line, statement->column, statement_index, "out of memory");
            return SQL_STATUS_OOM;
        }

        for (index = 0U; index < insert->column_count; index++) {
            int column_index = table_schema_find_column(schema, insert->column_names[index]);
            if (column_index < 0) {
                free(assigned);
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                sql_error_set(err, statement->line, statement->column, statement_index,
                              "unknown column '%s' in INSERT",
                              insert->column_names[index]);
                return SQL_STATUS_ERROR;
            }
            if (assigned[column_index]) {
                free(assigned);
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                sql_error_set(err, statement->line, statement->column, statement_index,
                              "duplicate column '%s' in INSERT",
                              insert->column_names[index]);
                return SQL_STATUS_ERROR;
            }

            status = ensure_type_match(schema->columns[column_index].type,
                                       &insert->values[index],
                                       statement->line,
                                       statement->column,
                                       statement_index,
                                       err);
            if (status != SQL_STATUS_OK) {
                free(assigned);
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                return status;
            }

            status = value_clone(&insert->values[index], &out_insert->row.values[column_index], err);
            if (status != SQL_STATUS_OK) {
                free(assigned);
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                return status;
            }
            assigned[column_index] = 1;
        }

        for (index = 0U; index < schema->column_count; index++) {
            if (!assigned[index]) {
                free(assigned);
                table_schema_free(&out_insert->schema);
                row_free(&out_insert->row);
                sql_error_set(err, statement->line, statement->column, statement_index,
                              "missing value for column '%s'",
                              schema->columns[index].name);
                return SQL_STATUS_ERROR;
            }
        }

        free(assigned);
    }

    return SQL_STATUS_OK;
}

static SqlStatus bind_select(const Statement *statement,
                             const TableSchema *schema,
                             BoundSelectStmt *out_select,
                             int statement_index,
                             SqlError *err) {
    const SelectStmt *select = &statement->as.select_stmt;
    size_t index;
    SqlStatus status;

    out_select->projection_count = 0U;
    out_select->projection_indices = NULL;
    out_select->has_filter = 0;
    value_init(&out_select->filter.value);

    status = table_schema_clone(schema, &out_select->schema, err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    if (select->select_all) {
        out_select->projection_count = schema->column_count;
        out_select->projection_indices = (size_t *)calloc(schema->column_count, sizeof(size_t));
        if (out_select->projection_indices == NULL) {
            table_schema_free(&out_select->schema);
            sql_error_set(err, statement->line, statement->column, statement_index, "out of memory");
            return SQL_STATUS_OOM;
        }

        for (index = 0U; index < schema->column_count; index++) {
            out_select->projection_indices[index] = index;
        }
    } else {
        out_select->projection_count = select->column_count;
        out_select->projection_indices = (size_t *)calloc(select->column_count, sizeof(size_t));
        if (out_select->projection_indices == NULL) {
            table_schema_free(&out_select->schema);
            sql_error_set(err, statement->line, statement->column, statement_index, "out of memory");
            return SQL_STATUS_OOM;
        }

        for (index = 0U; index < select->column_count; index++) {
            int column_index = table_schema_find_column(schema, select->column_names[index]);
            if (column_index < 0) {
                table_schema_free(&out_select->schema);
                free(out_select->projection_indices);
                out_select->projection_indices = NULL;
                sql_error_set(err, statement->line, statement->column, statement_index,
                              "unknown column '%s' in SELECT",
                              select->column_names[index]);
                return SQL_STATUS_ERROR;
            }
            out_select->projection_indices[index] = (size_t)column_index;
        }
    }

    if (select->where_clause != NULL) {
        const AstExpr *where = select->where_clause;
        const AstExpr *left;
        const AstExpr *right;
        int filter_index;

        if (where->kind != AST_EXPR_BINARY || where->as.binary.op != AST_BINARY_OP_EQ) {
            table_schema_free(&out_select->schema);
            free(out_select->projection_indices);
            out_select->projection_indices = NULL;
            sql_error_set(err, statement->line, statement->column, statement_index,
                          "only equality WHERE predicates are supported");
            return SQL_STATUS_ERROR;
        }

        left = where->as.binary.left;
        right = where->as.binary.right;
        if (left == NULL || right == NULL || left->kind != AST_EXPR_COLUMN_REF || right->kind != AST_EXPR_LITERAL) {
            table_schema_free(&out_select->schema);
            free(out_select->projection_indices);
            out_select->projection_indices = NULL;
            sql_error_set(err, statement->line, statement->column, statement_index,
                          "WHERE must have the form column = literal");
            return SQL_STATUS_ERROR;
        }

        filter_index = table_schema_find_column(schema, left->as.column_name);
        if (filter_index < 0) {
            table_schema_free(&out_select->schema);
            free(out_select->projection_indices);
            out_select->projection_indices = NULL;
            sql_error_set(err, statement->line, statement->column, statement_index,
                          "unknown column '%s' in WHERE clause",
                          left->as.column_name);
            return SQL_STATUS_ERROR;
        }

        status = ensure_type_match(schema->columns[filter_index].type,
                                   &right->as.literal,
                                   statement->line,
                                   statement->column,
                                   statement_index,
                                   err);
        if (status != SQL_STATUS_OK) {
            table_schema_free(&out_select->schema);
            free(out_select->projection_indices);
            out_select->projection_indices = NULL;
            return status;
        }

        status = value_clone(&right->as.literal, &out_select->filter.value, err);
        if (status != SQL_STATUS_OK) {
            table_schema_free(&out_select->schema);
            free(out_select->projection_indices);
            out_select->projection_indices = NULL;
            return status;
        }

        out_select->has_filter = 1;
        out_select->filter.column_index = (size_t)filter_index;
    }

    return SQL_STATUS_OK;
}

void bound_script_init(BoundScript *script) {
    if (script == NULL) {
        return;
    }

    script->statement_count = 0U;
    script->statements = NULL;
}

void bound_script_free(BoundScript *script) {
    size_t index;

    if (script == NULL) {
        return;
    }

    for (index = 0U; index < script->statement_count; index++) {
        BoundStatement *statement = &script->statements[index];

        if (statement->kind == BOUND_STATEMENT_INSERT) {
            table_schema_free(&statement->as.insert_stmt.schema);
            row_free(&statement->as.insert_stmt.row);
        } else if (statement->kind == BOUND_STATEMENT_SELECT) {
            table_schema_free(&statement->as.select_stmt.schema);
            free(statement->as.select_stmt.projection_indices);
            statement->as.select_stmt.projection_indices = NULL;
            value_free(&statement->as.select_stmt.filter.value);
        }
    }

    free(script->statements);
    script->statements = NULL;
    script->statement_count = 0U;
}

SqlStatus binder_bind_script(const Catalog *catalog, const AstScript *ast, BoundScript *out_script, SqlError *err) {
    size_t index;

    if (catalog == NULL || ast == NULL || out_script == NULL) {
        sql_error_set(err, 0, 0, 0, "binder_bind_script received null pointer");
        return SQL_STATUS_ERROR;
    }

    bound_script_init(out_script);

    for (index = 0U; index < ast->statement_count; index++) {
        const Statement *statement = &ast->statements[index];
        TableSchema schema;
        BoundStatement *grown;
        SqlStatus status;

        table_schema_init(&schema);
        status = clone_schema_from_catalog(catalog,
                                           statement->kind == STATEMENT_INSERT
                                               ? statement->as.insert_stmt.table_name
                                               : statement->as.select_stmt.table_name,
                                           &schema,
                                           statement->line,
                                           statement->column,
                                           (int)(index + 1U),
                                           err);
        if (status != SQL_STATUS_OK) {
            return status;
        }

        grown = (BoundStatement *)realloc(out_script->statements, sizeof(BoundStatement) * (out_script->statement_count + 1U));
        if (grown == NULL) {
            table_schema_free(&schema);
            bound_script_free(out_script);
            sql_error_set(err, statement->line, statement->column, (int)(index + 1U), "out of memory");
            return SQL_STATUS_OOM;
        }

        out_script->statements = grown;
        out_script->statements[out_script->statement_count].kind =
            (statement->kind == STATEMENT_INSERT) ? BOUND_STATEMENT_INSERT : BOUND_STATEMENT_SELECT;
        out_script->statements[out_script->statement_count].line = statement->line;
        out_script->statements[out_script->statement_count].column = statement->column;

        if (statement->kind == STATEMENT_INSERT) {
            status = bind_insert(statement,
                                 &schema,
                                 &out_script->statements[out_script->statement_count].as.insert_stmt,
                                 (int)(index + 1U),
                                 err);
        } else {
            status = bind_select(statement,
                                 &schema,
                                 &out_script->statements[out_script->statement_count].as.select_stmt,
                                 (int)(index + 1U),
                                 err);
        }

        table_schema_free(&schema);
        if (status != SQL_STATUS_OK) {
            bound_script_free(out_script);
            return status;
        }

        out_script->statement_count++;
    }

    return SQL_STATUS_OK;
}
