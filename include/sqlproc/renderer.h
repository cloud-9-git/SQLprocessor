#ifndef SQLPROC_RENDERER_H
#define SQLPROC_RENDERER_H

#include <stdio.h>

#include "sqlproc/executor.h"

/* SELECT 결과를 TSV 형식으로 출력합니다. */
SqlStatus renderer_print_results(FILE *stream, const ExecutionOutput *output, SqlError *err);

/* SqlError를 사용자용 오류 메시지로 출력합니다. */
void renderer_print_error(FILE *stream, const SqlError *err);

/* PlanScript를 사람이 읽기 쉬운 trace 형태로 출력합니다. */
void renderer_print_trace(FILE *stream, const PlanScript *plan);

/* --check 성공 메시지를 출력합니다. */
void renderer_print_check_ok(FILE *stream, size_t statement_count);

#endif
