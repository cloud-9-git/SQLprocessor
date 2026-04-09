#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "types.h"

/* INSERT Statement 실행: NN/PK/UK 제약 검사 후 레코드 추가 */
void execute_insert(Statement *stmt);

/* SELECT Statement 실행: 조건에 맞는 행 출력 */
void execute_select(Statement *stmt);

/* UPDATE Statement 실행: 조건 행 set 변경(제약 준수) */
void execute_update(Statement *stmt);

/* DELETE Statement 실행: 조건 행 삭제 */
void execute_delete(Statement *stmt);

/* 프로그램 종료 시 캐시 파일 핸들 정리 */
void close_all_tables(void);

#endif

