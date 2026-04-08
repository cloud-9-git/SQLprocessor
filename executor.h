#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "types.h"

// INSERT 실행 요청을 받아 NN/PK/UK 제약 검사 후 저장합니다.
void execute_insert(Statement *stmt);

// SELECT 실행 요청을 받아 조건 유무에 따라 전체/필터된 행을 출력합니다.
void execute_select(Statement *stmt);

// UPDATE 실행 요청을 받아 조건 행을 찾아 값 변경 후 파일을 갱신합니다.
void execute_update(Statement *stmt);

// DELETE 실행 요청을 받아 조건 행을 삭제하고 파일을 갱신합니다.
void execute_delete(Statement *stmt);

// 프로그램 종료 전 열린 테이블 파일 핸들을 모두 닫습니다.
void close_all_tables(void);

#endif
