# SQL 처리기

과제 요구사항에 맞춰 `INSERT`, `SELECT`만 지원하는 가벼운 C 기반 SQL 처리기입니다. SQL 파일을 커맨드라인 인자로 받아 한 줄씩 읽고, 각 테이블을 파일로 저장합니다.

## 지원 범위

- `INSERT INTO schema.table (col1, col2) VALUES (value1, value2);`
- `INSERT INTO schema.table VALUES (value1, value2);`
- `SELECT * FROM schema.table;`
- `SELECT col1, col2 FROM schema.table;`

## 동작 방식

- 입력: SQL 텍스트 파일
- 파싱: 한 줄에 하나의 SQL 문장을 파싱
- 실행:
  - `INSERT`는 `data/<schema>/<table>.csv` 파일에 행 추가
  - `SELECT`는 파일을 읽어서 콘솔에 출력
- `CREATE TABLE`은 지원하지 않음
- 스키마를 생략하면 `default`를 사용

첫 `INSERT`에서 컬럼 목록이 있으면 CSV 헤더를 만들 수 있어서 시연이 편합니다.

## 빌드와 실행

```sh
make
./sql_processor examples/demo.sql
```

임시 DB 경로를 따로 쓰고 싶다면:

```sh
./sql_processor examples/demo.sql tests/demo-db
```

## 예시 출력

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
```

## 저장 파일 예시

`data/academy/members.csv`

```text
id,name,role
1,Kim,backend
2,Lee,frontend
3,Park,platform
```

## 테스트

```sh
make test
```
