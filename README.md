# 📝 C언어 기반 간단한 SQL 처리기 (Simple SQL Processor)

텍스트 파일에 작성된 SQL 문(`INSERT`, `SELECT`)을 읽어 분석하고, CSV 파일 기반의 데이터베이스에 데이터를 저장 및 조회하는 가벼운 C언어 프로그램입니다. '수요 코딩회' 과제의 일환으로, 데이터베이스의 기본 동작 원리(파싱 및 실행)를 이해하기 위해 제작되었습니다.

## ✨ 주요 기능
- **자동 스키마 초기화**: 프로그램 실행 시 `users`, `products` 테이블(CSV 파일)과 헤더(컬럼명)를 자동으로 생성합니다.
- **SQL 파싱 (Parsing)**: 입력된 문자열을 분석하여 명령어(`INSERT`, `SELECT`), 테이블명, 데이터를 구조화합니다.
- **INSERT 지원**: 파일 기반 데이터베이스(CSV)에 새로운 레코드를 추가합니다.
- **SELECT 지원**: 지정된 테이블(CSV)의 모든 데이터를 읽어와 콘솔에 표 형태로 출력합니다.
- **대화형 CLI**: 프로그램 실행 후 읽어들일 SQL 텍스트 파일의 이름을 동적으로 입력받습니다.

## 🚀 시작하기 (Getting Started)

### 요구 사항
- C 컴파일러 (GCC 권장)
- 텍스트 형식의 SQL 쿼리 파일 (예: `queries.txt`)

### 컴파일 방법
터미널에서 소스 코드가 있는 디렉토리로 이동한 후, 아래 명령어를 통해 컴파일합니다.
```bash
gcc sql.c -o sql_processor
```

### 실행 방법
```bash
# Mac/Linux
./sql_processor

# Windows (CMD/PowerShell)
sql_processor
```
프로그램이 실행되면 프롬프트의 안내에 따라 준비된 SQL 텍스트 파일의 이름(예: `queries.txt`)을 입력합니다.

## 📄 입력 파일 예시 (`queries.txt`)
실행 전, 프로젝트 폴더에 아래와 같은 문법으로 작성된 텍스트 파일을 준비해 주세요.
```sql
INSERT INTO users VALUES (1, Hong_Gildong, 25);
INSERT INTO users VALUES (2, Kim_Chulsoo, 30);
SELECT * FROM users;
INSERT INTO products VALUES (101, Laptop, 1500);
SELECT * FROM products;
```

## 💻 실행 결과 화면
프로그램은 쿼리를 순차적으로 실행하며 성공 여부와 조회 결과를 출력합니다.

```text
--- 데이터베이스 초기화 중 ---
[시스템] 'users.csv' 테이블이 없어 새로 생성했습니다. (스키마: id,name,age)
[시스템] 'products.csv' 테이블이 없어 새로 생성했습니다. (스키마: id,product_name,price)
--- 데이터베이스 준비 완료 ---

실행할 SQL 파일 이름을 입력하세요 (예: queries.txt): queries.txt

--- [queries.txt] 파일 읽기 시작 ---

[실행 중인 SQL] INSERT INTO users VALUES (1, Hong_Gildong, 25);
[성공] users 테이블에 데이터를 저장했습니다. (1, Hong_Gildong, 25)

[실행 중인 SQL] SELECT * FROM users;
=== [users] 테이블 조회 결과 ===
- id,name,age
- 1, Hong_Gildong, 25
===============================
```

## 📁 프로젝트 파일 구조
- `sql.c` : 프로그램의 메인 소스 코드 (파서, 실행기 로직 포함)
- `queries.txt` : 실행할 SQL 명령어들이 담긴 텍스트 파일 (사용자 작성)
- `users.csv` / `products.csv` : 프로그램 실행 후 데이터가 물리적으로 저장되는 파일 DB