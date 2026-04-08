# SQLprocessor

CSV를 간단한 테이블처럼 다루는 경량 SQL 엔진입니다. `SELECT *`, `INSERT`, `UPDATE`, `DELETE`와 `(PK)/(UK)/(NN)` 제약을 지원합니다.

## 1) 한눈에 보는 동작

- 시작: `./sqlsprocessor <sql-file>` 실행
- 처리: SQL 파일을 `;` 기준으로 문장 분리 (`'...'` 내부 세미콜론은 제외)
- 실행: 문장을 파싱해 `SELECT/INSERT/UPDATE/DELETE` 중 하나로 분기
- 결과: 메모리 캐시에서 작업 후 변경사항은 파일 전체 재기록

## 2) 실행 순서

1. `main`에서 파일을 라인/문자 단위로 읽음
2. 주석(`--`) 제거, `;`로 문장 단위 추출
3. `parse_statement`로 Statement 생성
4. `get_table`로 대상 CSV 테이블 캐시 확보
5. 문장 타입별 실행 함수 호출
6. 필요 시 `rewrite_file`로 저장

## 3) 지원 SQL

```sql
SELECT * FROM table_name [WHERE col = value];
INSERT INTO table_name VALUES (v1, v2, ...);
UPDATE table_name SET col = value [WHERE col = value];
DELETE FROM table_name WHERE col = value;
```

## 4) CSV 스키마 예시와 제약

```csv
id(PK),email(UK),phone(UK),pwd(NN),name
```

- `PK`: 중복 불가(INSERT 시 중복 거부)
- `UK`: 중복 불가(INSERT/해당 UK UPDATE 시 중복 거부)
- `NN`: 빈 값 불가

## 5) 핵심 구성요소 (중요 함수만)

### 파싱/문장 해석

- `parse_statement`: SQL 한 문장을 `Statement`로 변환
- `get_next_token`/`parse_*`: 키워드/식별자/값 추출 및 문법 판별
- `parse_where_clause`: `WHERE col = value` 처리

### 테이블/캐시 관리

- `get_table`: `<table>.csv` 로드 후 컬럼·제약·레코드 캐시 생성 또는 재사용
- `parse_csv_row`: CSV 한 줄을 컬럼별 값으로 분해
- `get_col_idx`: 컬럼명으로 인덱스 탐색
- `rewrite_file`: 캐시 상태를 CSV에 전면 저장

### 실행(데이터 조작)

- `execute_select`: 조건 유무에 따라 전체/필터 조회
- `execute_insert`: NN/PK/UK 검사 후 삽입
- `execute_update`: 조건 행 갱신, PK 변경 불가, NN/UK 검사
- `execute_delete`: 조건 행 삭제

### 유틸

- `trim_and_unquote`: 공백/따옴표 정리
- `compare_value`: 비교 전 정규화 후 문자열 비교
- `find_in_pk_index`: PK 중복 탐지 보조

## 6) 테스트 권장 순서

```bash
./sqlsprocessor case_basic_users_reset.txt
./sqlsprocessor case_basic_users_run.txt

./sqlsprocessor case_constraints_users_reset.txt
./sqlsprocessor case_constraints_users_run.txt

./sqlsprocessor case_quotes_users_reset.txt
./sqlsprocessor case_quotes_users_run.txt

./sqlsprocessor case_products_items_reset.txt
./sqlsprocessor case_products_items_run.txt

./sqlsprocessor case_invalid_users_reset.txt
./sqlsprocessor case_invalid_users_run.txt
```

`run`은 동작 시나리오, `reset`은 상태 복원 시나리오입니다.

## 7) 핵심 제약사항

- 입력 SQL 문법은 제한적이며, 조인/다중 조건/정렬/집계는 미지원
- 최대치 초과(`MAX_RECORDS`, `MAX_SQL_LEN`) 시 안정성 저하 가능
