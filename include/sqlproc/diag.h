#ifndef SQLPROC_DIAG_H
#define SQLPROC_DIAG_H

#include <stdarg.h>

/* 함수들이 성공/실패를 공통된 방식으로 반환하기 위한 상태 코드입니다. */
typedef enum {
    SQL_STATUS_OK = 0,
    SQL_STATUS_ERROR = 1,
    SQL_STATUS_OOM = 2,
    SQL_STATUS_IO = 3
} SqlStatus;

/* SQL 처리 중 발생한 오류를 사용자에게 보여주기 위한 진단 정보입니다.
 * 어느 statement에서, 몇 번째 줄/열에서 문제가 났는지 함께 담습니다.
 */
typedef struct {
    int line;
    int column;
    int statement_index;
    char message[256];
} SqlError;

/* 오류 구조체를 빈 상태로 초기화합니다. */
void sql_error_init(SqlError *err);

/* 가변 인자를 받아 오류 메시지와 위치 정보를 채웁니다. */
void sql_error_set(SqlError *err, int line, int column, int statement_index, const char *fmt, ...);

/* 이미 준비된 va_list를 받아 오류를 채우는 내부 공용 함수입니다. */
void sql_error_vset(SqlError *err, int line, int column, int statement_index, const char *fmt, va_list args);

/* 상태 코드를 사람이 읽기 쉬운 문자열로 바꿉니다. */
const char *sql_status_name(SqlStatus status);

#endif
