# SQLprocessor

CSV 파일을 테이블처럼 다루는 간단한 C 기반 SQL 처리기입니다.

지원 범위:
- `INSERT`
- `SELECT *`
- `UPDATE`
- `DELETE`
- `PK`, `UK`, `NN` 제약 일부 검증
- `WHERE column = value` 형태 조건식

## Build

```bash
gcc sqlsprocessor.c -o sqlsprocessor
```

실행:

```bash
./sqlsprocessor case_basic_users_run.txt
```

인자를 주지 않으면 실행 중에 SQL 파일명을 입력받습니다.

## Table Schema

CSV 첫 줄은 스키마입니다.

예시:

```csv
id(PK),email(UK),phone(UK),pwd(NN),name
```

의미:
- `PK`: Primary Key
- `UK`: Unique Key
- `NN`: Not Null

## Test Files

이번 테스트 세트는 파일명만 보고도 어떤 `txt`가 어떤 `csv`를 사용하는지 알 수 있게 맞춰 두었습니다.

규칙:
- `case_name.csv`: 테스트 대상 데이터
- `case_name_run.txt`: 실행용 SQL
- `case_name_reset.txt`: 원래 상태로 복구하는 SQL

현재 포함된 케이스:
- `case_basic_users`
- `case_constraints_users`
- `case_quotes_users`
- `case_products_items`
- `case_invalid_users`

## Test Matrix

### 1. Basic CRUD

파일:
- `case_basic_users.csv`
- `case_basic_users_run.txt`
- `case_basic_users_reset.txt`

목적:
- 기본 `INSERT`, `SELECT`, `UPDATE`, `DELETE` 확인

실행:

```bash
./sqlsprocessor case_basic_users_run.txt
./sqlsprocessor case_basic_users_reset.txt
```

### 2. Constraint Check

파일:
- `case_constraints_users.csv`
- `case_constraints_users_run.txt`
- `case_constraints_users_reset.txt`

목적:
- PK 중복
- UK 중복
- NN 위반
- PK 컬럼 `UPDATE` 금지 여부

실행:

```bash
./sqlsprocessor case_constraints_users_run.txt
./sqlsprocessor case_constraints_users_reset.txt
```

### 3. Quoted Value

파일:
- `case_quotes_users.csv`
- `case_quotes_users_run.txt`
- `case_quotes_users_reset.txt`

목적:
- 따옴표 포함 값
- 값 내부 쉼표 처리
- quoted value로 `WHERE` / `UPDATE`

실행:

```bash
./sqlsprocessor case_quotes_users_run.txt
./sqlsprocessor case_quotes_users_reset.txt
```

### 4. Different Schema

파일:
- `case_products_items.csv`
- `case_products_items_run.txt`
- `case_products_items_reset.txt`

목적:
- `users` 외 다른 스키마에서도 동작하는지 확인

실행:

```bash
./sqlsprocessor case_products_items_run.txt
./sqlsprocessor case_products_items_reset.txt
```

### 5. Invalid Query

파일:
- `case_invalid_users.csv`
- `case_invalid_users_run.txt`
- `case_invalid_users_reset.txt`

목적:
- 지원하지 않는 SQL 문법 입력 시 처리 확인

실행:

```bash
./sqlsprocessor case_invalid_users_run.txt
./sqlsprocessor case_invalid_users_reset.txt
```

## Recommended Test Order

초기화 후 실행하는 흐름을 권장합니다.

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

## Notes

- 각 `run` 파일은 대응되는 `csv`를 직접 수정합니다.
- 같은 케이스를 반복 시험할 때는 먼저 대응되는 `reset` 파일을 실행하면 됩니다.
- 현재 프로젝트 루트에 있던 기존 `users.csv`, `test.txt`는 별도 수동 테스트용으로 그대로 둘 수 있습니다.
