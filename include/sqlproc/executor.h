#ifndef SQLPROC_EXECUTOR_H
#define SQLPROC_EXECUTOR_H

#include <stdio.h>

#include "sqlproc/plan.h"

/* SELECT 한 번의 결과를 담는 표 형태 결과셋입니다. */
typedef struct {
    size_t column_count;
    char **column_names;
    size_t row_count;
    Row *rows;
} ResultSet;

/* SQL 파일 전체 실행 결과입니다.
 * SELECT가 여러 번 나오면 ResultSet도 여러 개 생깁니다.
 */
typedef struct {
    size_t result_count;
    ResultSet *results;
} ExecutionOutput;

/* executor가 실행 중 참고하는 환경 정보입니다. */
typedef struct {
    const char *db_root;
    int trace;
    FILE *trace_stream;
} ExecutionContext;

/* ExecutionOutput을 빈 상태로 초기화합니다. */
void execution_output_init(ExecutionOutput *output);

/* ExecutionOutput 안의 결과셋과 row 메모리를 모두 해제합니다. */
void execution_output_free(ExecutionOutput *output);

/* 🧭 PlanScript를 실제로 실행해 INSERT는 저장하고 SELECT는 결과셋으로 모읍니다. */
SqlStatus executor_run_script(ExecutionContext *ctx, const PlanScript *plan, ExecutionOutput *out_output, SqlError *err);

#endif
