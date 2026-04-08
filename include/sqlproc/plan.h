#ifndef SQLPROC_PLAN_H
#define SQLPROC_PLAN_H

#include <stddef.h>

#include "sqlproc/schema.h"

/* planner가 만들 수 있는 논리 실행 노드 종류입니다. */
typedef enum {
    PLAN_NODE_INSERT = 0,
    PLAN_NODE_SEQ_SCAN = 1,
    PLAN_NODE_FILTER = 2,
    PLAN_NODE_PROJECT = 3,
    PLAN_NODE_INDEX_SCAN = 4,
    PLAN_NODE_LIMIT = 5,
    PLAN_NODE_SORT = 6
} PlanNodeKind;

typedef struct PlanNode PlanNode;

/* 논리 실행 계획의 한 노드입니다. */
struct PlanNode {
    PlanNodeKind kind;
    /* ⭐ v1의 SELECT 계획은 `PROJECT -> FILTER -> SEQ_SCAN` 같은 단일 체인입니다. */
    PlanNode *child;
    union {
        /* INSERT는 저장할 대상 테이블과 row를 직접 들고 있습니다. */
        struct {
            char *table_name;
            Row row;
        } insert;
        /* SEQ_SCAN은 어떤 테이블을 순차 탐색할지만 표현합니다. */
        struct {
            char *table_name;
        } seq_scan;
        /* FILTER는 어떤 컬럼이 어떤 값과 같은지 검사합니다. */
        struct {
            size_t column_index;
            Value value;
        } filter;
        /* PROJECT는 출력할 컬럼 순서를 schema index 배열로 가집니다. */
        struct {
            size_t projection_count;
            size_t *projection_indices;
        } project;
    } as;
};

/* 계획 단계에서 구분하는 statement 종류입니다. */
typedef enum {
    PLAN_STATEMENT_INSERT = 0,
    PLAN_STATEMENT_SELECT = 1
} PlanStatementKind;

/* 실행 가능한 형태로 변환된 statement 하나입니다. */
typedef struct {
    PlanStatementKind kind;
    int line;
    int column;
    /* ⭐ 실행 단계에서 스키마 파일을 다시 열 필요가 없도록 schema를 복사해 둡니다. */
    TableSchema schema;
    PlanNode *root;
} PlanStatement;

/* SQL 파일 전체에 대한 실행 계획 묶음입니다. */
typedef struct {
    size_t statement_count;
    PlanStatement *statements;
} PlanScript;

/* PlanScript를 빈 상태로 초기화합니다. */
void plan_script_init(PlanScript *script);

/* PlanScript 안의 schema와 plan tree를 모두 해제합니다. */
void plan_script_free(PlanScript *script);

/* PlanNodeKind를 trace 출력용 문자열로 바꿉니다. */
const char *plan_node_kind_name(PlanNodeKind kind);

#endif
