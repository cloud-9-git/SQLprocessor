#include "sqlproc/binder.h"
#include "sqlproc/catalog.h"
#include "sqlproc/executor.h"
#include "sqlproc/parser.h"
#include "sqlproc/planner.h"
#include "sqlproc/renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CLI에서 받은 실행 옵션을 한곳에 모아두는 구조체입니다. */
typedef struct {
    const char *db_root;
    const char *input_path;
    int check_only;
    int trace;
} CliOptions;

/* 잘못된 인자를 입력했을 때 보여줄 사용법입니다. */
static void print_usage(FILE *stream) {
    fputs("usage: sqlproc --db-root <dir> --input <sql-file> [--check] [--trace]\n", stream);
}

/* SQL 파일 전체를 메모리로 읽어 parser에 넘길 문자열을 준비합니다. */
static SqlStatus read_file(const char *path, char **out_text, SqlError *err) {
    FILE *file;
    long size;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        sql_error_set(err, 0, 0, 0, "failed to open input file '%s'", path);
        return SQL_STATUS_IO;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        sql_error_set(err, 0, 0, 0, "failed to seek input file '%s'", path);
        return SQL_STATUS_IO;
    }

    size = ftell(file);
    if (size < 0L) {
        fclose(file);
        sql_error_set(err, 0, 0, 0, "failed to determine size for '%s'", path);
        return SQL_STATUS_IO;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        sql_error_set(err, 0, 0, 0, "failed to rewind input file '%s'", path);
        return SQL_STATUS_IO;
    }

    buffer = (char *)malloc((size_t)size + 1U);
    if (buffer == NULL) {
        fclose(file);
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    if (size > 0L && fread(buffer, 1U, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        sql_error_set(err, 0, 0, 0, "failed to read input file '%s'", path);
        return SQL_STATUS_IO;
    }
    buffer[(size_t)size] = '\0';

    if (fclose(file) != 0) {
        free(buffer);
        sql_error_set(err, 0, 0, 0, "failed to close input file '%s'", path);
        return SQL_STATUS_IO;
    }

    *out_text = buffer;
    return SQL_STATUS_OK;
}

/* CLI 인자를 해석해 엔진이 사용할 옵션 구조체로 바꿉니다. */
static SqlStatus parse_args(int argc, char **argv, CliOptions *out_options, SqlError *err) {
    int index;

    out_options->db_root = NULL;
    out_options->input_path = NULL;
    out_options->check_only = 0;
    out_options->trace = 0;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--db-root") == 0) {
            if (index + 1 >= argc) {
                sql_error_set(err, 0, 0, 0, "missing value for --db-root");
                return SQL_STATUS_ERROR;
            }
            out_options->db_root = argv[++index];
        } else if (strcmp(argv[index], "--input") == 0) {
            if (index + 1 >= argc) {
                sql_error_set(err, 0, 0, 0, "missing value for --input");
                return SQL_STATUS_ERROR;
            }
            out_options->input_path = argv[++index];
        } else if (strcmp(argv[index], "--check") == 0) {
            out_options->check_only = 1;
        } else if (strcmp(argv[index], "--trace") == 0) {
            out_options->trace = 1;
        } else {
            sql_error_set(err, 0, 0, 0, "unknown argument '%s'", argv[index]);
            return SQL_STATUS_ERROR;
        }
    }

    if (out_options->db_root == NULL || out_options->input_path == NULL) {
        sql_error_set(err, 0, 0, 0, "both --db-root and --input are required");
        return SQL_STATUS_ERROR;
    }

    return SQL_STATUS_OK;
}

/* 🧭 프로그램의 실제 진입점입니다.
 * `파일 읽기 -> 파싱 -> 바인딩 -> 플래닝 -> 실행 -> 출력` 순서로 호출됩니다.
 */
int main(int argc, char **argv) {
    CliOptions options;
    Catalog catalog;
    AstScript ast;
    BoundScript bound;
    PlanScript plan;
    ExecutionContext exec_ctx;
    ExecutionOutput output;
    SqlError err;
    SqlStatus status;
    char *sql_text = NULL;

    sql_error_init(&err);
    ast_script_init(&ast);
    bound_script_init(&bound);
    plan_script_init(&plan);
    execution_output_init(&output);

    /* 🧭 CLI 실행은 항상 같은 파이프라인을 따릅니다.
     * file -> parse -> bind -> plan -> execute -> render
     */
    status = parse_args(argc, argv, &options, &err);
    if (status != SQL_STATUS_OK) {
        print_usage(stderr);
        renderer_print_error(stderr, &err);
        return 1;
    }

    status = read_file(options.input_path, &sql_text, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        return 1;
    }

    status = parser_parse_script(sql_text, &ast, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        free(sql_text);
        return 1;
    }

    catalog_init(&catalog, options.db_root);
    status = binder_bind_script(&catalog, &ast, &bound, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        free(sql_text);
        ast_script_free(&ast);
        return 1;
    }

    status = planner_build_script(&bound, &plan, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        free(sql_text);
        ast_script_free(&ast);
        bound_script_free(&bound);
        return 1;
    }

    if (options.trace) {
        renderer_print_trace(stderr, &plan);
    }

    /* ⭐ --check는 실제 실행 전에 멈추므로 데이터 파일을 변경하지 않습니다. */
    if (options.check_only) {
        renderer_print_check_ok(stdout, plan.statement_count);
        free(sql_text);
        ast_script_free(&ast);
        bound_script_free(&bound);
        plan_script_free(&plan);
        return 0;
    }

    exec_ctx.db_root = options.db_root;
    exec_ctx.trace = options.trace;
    exec_ctx.trace_stream = stderr;
    status = executor_run_script(&exec_ctx, &plan, &output, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        free(sql_text);
        ast_script_free(&ast);
        bound_script_free(&bound);
        plan_script_free(&plan);
        execution_output_free(&output);
        return 1;
    }

    status = renderer_print_results(stdout, &output, &err);
    if (status != SQL_STATUS_OK) {
        renderer_print_error(stderr, &err);
        free(sql_text);
        ast_script_free(&ast);
        bound_script_free(&bound);
        plan_script_free(&plan);
        execution_output_free(&output);
        return 1;
    }

    free(sql_text);
    ast_script_free(&ast);
    bound_script_free(&bound);
    plan_script_free(&plan);
    execution_output_free(&output);
    return 0;
}
