# SQLprocessor

파일 기반 DB 위에서 동작하는 C 기반 SQL 처리기입니다. SQL 파일을 CLI로 입력받아 `파싱 -> 의미 검증 -> 실행 계획 생성 -> 실행 -> 파일 저장/조회` 흐름으로 처리합니다.

## Goals
- 과제 범위를 안정적으로 완성합니다.
- 이후 `UPDATE`, `DELETE`, `ORDER BY`, 인덱스, 바이너리 저장소로 확장할 수 있도록 내부 계층을 분리합니다.
- 테스트와 문서를 포함해 README만으로 발표와 데모가 가능하도록 구성합니다.

## Supported SQL
- `INSERT INTO table VALUES (...);`
- `INSERT INTO table (col1, col2, ...) VALUES (...);`
  - v1에서는 부분 컬럼 INSERT를 허용하지 않습니다. 명시 컬럼을 쓰더라도 스키마의 모든 컬럼을 제공해야 합니다.
- `SELECT * FROM table;`
- `SELECT col1, col2 FROM table;`
- `SELECT ... FROM table WHERE column = literal;`

지원 literal:
- `INT`
- `TEXT` via `'single quoted string'`
- `BOOL` via `true` / `false`

## Architecture
외부 기능은 작게 두고, 내부는 다음 계층으로 분리했습니다.

1. `lexer/parser`
   - SQL 파일을 토큰과 AST로 변환합니다.
2. `binder`
   - 스키마를 읽어 테이블/컬럼/타입을 검증하고 실행 친화적인 구조로 정리합니다.
3. `planner`
   - bound statement를 logical plan으로 변환합니다.
   - v1은 `INSERT`, `SEQ_SCAN`, `FILTER`, `PROJECT`만 사용합니다.
4. `executor`
   - logical plan을 실행하고 결과셋을 만듭니다.
5. `storage/catalog`
   - schema 파일과 row 파일을 읽고 씁니다.

이 구조 덕분에 이후 성능 개선은 외부 인터페이스를 깨지 않고 내부 구현을 교체하는 방식으로 진행할 수 있습니다.

## Reading Guide
처음 보는 사람이라면 아래 순서로 읽는 것이 가장 이해가 쉽습니다.

1. 큰 그림부터 파악
   - `README.md`
   - `docs/architecture.md`
   - 무엇을 지원하는지, 계층이 왜 나뉘었는지 먼저 봅니다.
2. 실제 실행 진입점 확인
   - `src/main.c`
   - `file -> parse -> bind -> plan -> execute -> render` 파이프라인이 어떻게 연결되는지 봅니다.
3. SQL이 구조화되는 과정 확인
   - `src/lexer.c`
   - `src/parser.c`
   - SQL 문자열이 토큰과 AST로 바뀌는 지점을 봅니다.
4. 의미 검증과 실행 계획 생성 확인
   - `src/binder.c`
   - `src/planner.c`
   - 컬럼 이름이 schema index로 바뀌고, `PROJECT -> FILTER -> SEQ_SCAN` plan이 만들어지는 흐름을 봅니다.
5. 실제 파일 읽기/쓰기와 실행 확인
   - `src/storage.c`
   - `src/executor.c`
   - row가 파일에 어떻게 저장되고, SELECT 결과가 어떻게 만들어지는지 봅니다.
6. 데이터 구조를 옆에서 함께 확인
   - `include/sqlproc/ast.h`
   - `include/sqlproc/plan.h`
   - `include/sqlproc/schema.h`
   - 각 단계에서 어떤 데이터 구조를 주고받는지 같이 보면 흐름이 더 빨리 잡힙니다.
7. 마지막으로 테스트 확인
   - `tests/unit/`
   - `tests/integration/test_cli.sh`
   - 구현을 읽고 난 뒤 테스트를 보면 의도한 동작과 edge case를 한 번에 정리할 수 있습니다.

짧게만 훑고 싶다면 `src/main.c -> src/parser.c -> src/binder.c -> src/planner.c -> src/executor.c -> src/storage.c` 순서만 읽어도 전체 흐름이 잡힙니다.

## Storage Layout
DB root는 아래처럼 구성합니다.

```text
<db-root>/
├── schema/
│   └── users.schema
└── data/
    └── users.rows
```

스키마 파일 형식:

```text
version 1
table users
column id INT
column name TEXT
column active BOOL
```

데이터 파일 형식:
- row-per-line escaped TSV
- 컬럼 순서는 schema 기준
- escape 지원: `\\`, `\t`, `\n`

## Build
```sh
make debug
make release
```

## Test
```sh
make test
make test-sanitize
```

테스트 구성:
- unit tests: lexer/parser/binder/planner/storage/executor
- integration tests: CLI 기준 출력, 오류, 파일 변경 여부 검증

## CLI
```sh
build/sqlproc --db-root <dir> --input <sql-file> [--check] [--trace]
```

옵션:
- `--check`: parse/bind/plan까지만 수행하고 실제 파일은 변경하지 않습니다.
- `--trace`: statement별 logical plan을 stderr에 출력합니다.

## Demo
예제 DB와 SQL은 [`examples/`](/Users/hi/Library/CloudStorage/Dropbox/Mac/Desktop/Jungle/SQLprocessor/examples) 아래에 있습니다.

검증만 수행:
```sh
build/sqlproc --db-root examples/db --input examples/mixed.sql --check
```

실행:
```sh
build/sqlproc --db-root examples/db --input examples/select.sql
```

trace 포함 실행:
```sh
build/sqlproc --db-root examples/db --input examples/mixed.sql --trace
```

## Sample Output
```text
id	name
1	Alice
3	Charlie
```

## Edge Cases Covered
- 세미콜론 누락
- 존재하지 않는 테이블/컬럼
- 타입 불일치
- 다중 문장 파일 실행
- `--check` 무변경 보장
- escaped text round-trip

## Limitations
- `CREATE TABLE`, `UPDATE`, `DELETE`, `JOIN`, `ORDER BY`, `LIMIT`, aggregate는 미구현입니다.
- `NULL` 미지원
- 트랜잭션/동시성/인덱스/optimizer 미지원
- v1은 statement-level autocommit입니다.

## Roadmap
- v2: `UPDATE`, `DELETE`, `AND/OR`, `LIMIT`
- v3: `EXPLAIN`, `ORDER BY`
- v4: `IndexScan`, 바이너리 페이지 저장소
- v5: 간단한 optimizer

## Presentation Flow
4분 발표 기준 추천 흐름:

1. 문제 정의: SQL 파일을 파일 기반 DB로 처리하는 구조 설명
2. 아키텍처: `Parser -> Binder -> Planner -> Executor -> Storage`
3. 데모: `SELECT`, `INSERT + SELECT`, `--check`, `--trace`
4. 검증: `make test`, edge case, sanitizer
5. 확장 포인트: 인덱스/바이너리 저장소/optimizer
