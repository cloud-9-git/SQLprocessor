# Architecture

## Processing Flow
```text
SQL file
  -> lexer
  -> parser
  -> AST
  -> binder
  -> bound script
  -> planner
  -> logical plan
  -> executor
  -> storage/catalog
  -> stdout/stderr
```

## Module Responsibilities
- `diag`
  - 공통 상태 코드와 에러 포맷
- `value`
  - `INT`, `TEXT`, `BOOL` 값 모델과 비교/문자열화
- `catalog`
  - `<db-root>/schema/*.schema` 로드
  - `TableSchema`, `Row` 메모리 관리 유틸 포함
- `lexer`
  - keyword, identifier, literal, punctuation 토큰화
  - line/column 추적
- `parser`
  - AST 생성
  - 다중 statement 지원
- `binder`
  - 스키마 기반 semantic validation
  - 컬럼 이름을 schema index로 resolve
  - INSERT row를 schema 순서로 정렬
- `planner`
  - `INSERT`, `SEQ_SCAN`, `FILTER`, `PROJECT` plan 구성
- `storage`
  - escaped TSV row append/scan
- `executor`
  - logical plan 실행과 result set 생성
- `renderer`
  - TSV 결과, 에러, trace, check 메시지 출력

## Why This Split Matters
- AST를 직접 실행하지 않으므로 이후 logical plan 확장이 쉽습니다.
- storage 포맷이 executor 위로 새지 않으므로 텍스트 저장소를 바이너리 저장소로 교체할 수 있습니다.
- binder가 semantic validation을 담당하므로 executor는 실행에 집중할 수 있습니다.

## Current Plan Nodes
- `PLAN_NODE_INSERT`
- `PLAN_NODE_SEQ_SCAN`
- `PLAN_NODE_FILTER`
- `PLAN_NODE_PROJECT`

미구현이지만 enum에 예약된 노드:
- `PLAN_NODE_INDEX_SCAN`
- `PLAN_NODE_LIMIT`
- `PLAN_NODE_SORT`

## Failure Model
- parse/bind/plan 단계에서 실패하면 실행하지 않습니다.
- 실행 도중 실패하면 현재 문장에서 중단합니다.
- 이전 statement는 이미 반영된 상태로 유지됩니다.

## Output Contract
- SELECT 결과는 TSV 헤더 + rows
- 여러 SELECT 결과는 빈 줄로 구분
- 오류는 `statement`, `line`, `column` 포함
