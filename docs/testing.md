# Testing

## Test Layers
- unit test
  - lexer
  - parser
  - binder
  - planner
  - storage
  - executor
- integration test
  - 실제 CLI 실행
  - fixture DB 복사 후 SQL 파일 입력
  - stdout, stderr, row file 변경 여부 검증

## Main Scenarios
- `INSERT -> SELECT`
- projection SELECT
- `WHERE column = literal`
- schema order와 다른 explicit INSERT column order
- type mismatch
- invalid SQL
- `--check` mode no-op
- escaped text round-trip

## Commands
```sh
make test
make test-sanitize
```

## Validation Strategy
- unit test로 함수와 모듈 경계를 먼저 고정
- integration test로 사용자 관점 동작 보장
- sanitizer로 메모리 오류와 UB를 추가 점검
