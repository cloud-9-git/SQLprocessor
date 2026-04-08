# SQL 처리기

수요 코딩회 과제 요구사항에 맞춰 만든 C 기반 SQL 처리기입니다. 입력으로 받은 SQL 텍스트 파일을 파싱한 뒤 실행하고, 각 테이블을 파일로 저장합니다.

## 구현 범위

- 지원 SQL
  - `INSERT`
  - `SELECT`
  - 추가 구현: `SELECT ... WHERE column = value`
- 실행 흐름
  - `입력(SQL 파일) -> 파싱(AST 생성) -> 실행 -> 파일 저장/조회`
- 저장 방식
  - 각 테이블을 `data/<schema>/<table>.table` 파일로 관리
  - 내부 포맷은 커스텀 텍스트 포맷(`@columns`, `@row`) 사용
- 전제
  - `CREATE TABLE`은 구현하지 않음
  - 스키마/테이블은 이미 있다고 가정하되, 편의상 첫 `INSERT` 시 파일이 없으면 생성 가능

## 지원 문법

```sql
INSERT INTO schema.table (col1, col2) VALUES (value1, value2);
INSERT INTO schema.table VALUES (value1, value2);
SELECT * FROM schema.table;
SELECT col1, col2 FROM schema.table;
SELECT col1 FROM schema.table WHERE col2 = 'value';
```

- 스키마를 생략하면 기본값으로 `default`를 사용합니다.
- SQL 파일 안에 여러 문장을 `;`로 이어서 넣을 수 있습니다.
- `-- 주석`을 지원합니다.
- 문자열은 `'text'` 형식이며 SQL 방식의 `''` 이스케이프를 지원합니다.
- 추가 구현으로 `WHERE column = value` 형태의 단일 동등 비교를 지원합니다.

## 빌드

```sh
make
```

## 실행

```sh
./sql_processor examples/demo.sql
```

같은 `data` 디렉터리로 반복 실행하면 `INSERT` 결과가 계속 누적됩니다.

다른 데이터 루트를 쓰고 싶다면 두 번째 인자로 경로를 줄 수 있습니다.

```sh
./sql_processor examples/demo.sql ./my-db
```

## 출력 예시

```text
INSERT 1 academy.members
INSERT 1 academy.members
INSERT 1 academy.members
RESULT academy.members
id | name | role
1 | Kim | backend
2 | Lee | frontend
3 | Park | platform
(3 rows)
RESULT academy.members
name | role
Kim | backend
Lee | frontend
Park | platform
(3 rows)
RESULT academy.members
id | name
2 | Lee
(1 rows)
```

## 파일 저장 포맷

테이블 파일은 다음처럼 저장됩니다.

```text
@columns|id|name|role
@row|1|Kim|backend
@row|2|Lee|frontend
```

- 헤더(`@columns`)는 컬럼 순서를 정의합니다.
- 데이터(`@row`)는 같은 순서로 값을 저장합니다.
- 줄바꿈, 탭, `|`, `\` 문자는 이스케이프하여 저장합니다.

## 테스트

```sh
make test
```

## 핵심 설계 포인트

- 파서와 실행기를 분리해서 SQL 분석 결과를 구조체(AST)로 보관합니다.
- 파일 기반 DB이므로 실행기는 테이블 파일을 읽어 메모리 구조로 올린 뒤 갱신하고 다시 저장합니다.
- `INSERT ... VALUES (...)` 와 `INSERT ... (columns) VALUES (...)` 둘 다 지원합니다.
- `SELECT`는 전체 컬럼(`*`) 또는 특정 컬럼 projection 을 지원합니다.
- `WHERE`는 현재 단일 컬럼에 대한 `=` 비교를 지원합니다.
