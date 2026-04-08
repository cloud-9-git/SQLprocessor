# SQLprocessor 발표 대본

## 발표 전 준비

아래 빌드는 발표 시작 전에 미리 끝내둡니다.

```bash
gcc -fdiagnostics-color=always -g main.c lexer.c parser.c executor.c -o sqlproc_demo
```

## 3분 30초 대본

### 0:00 - 0:20

안녕하세요. 저희는 C로 SQL 파일을 입력받아 파싱하고, 실행하고, CSV 파일에 저장하는 SQL Processor를 구현했습니다.  
이번 과제의 핵심은 단순 저장이 아니라 `입력 -> 파싱 -> 실행 -> 저장` 흐름을 직접 만드는 것이었습니다.

### 0:20 - 0:50

README의 구조도를 보시면, CLI로 받은 SQL 파일이 `main.c`를 거쳐 문장 단위로 분리되고,  
그다음 `lexer.c`, `parser.c`를 통해 `Statement` 구조체로 바뀝니다.  
마지막으로 `executor.c`가 SELECT, INSERT, UPDATE, DELETE를 실행하고 CSV 파일에 반영합니다.

### 0:50 - 1:10

필수 요구사항은 INSERT와 SELECT였지만, 저희는 UPDATE, DELETE와 PK, UK, NN 제약조건까지 추가로 구현했습니다.  
그래서 단순 조회기가 아니라 작은 CRUD 처리기로 볼 수 있습니다.

### 1:10 - 2:05

먼저 정상 동작 테스트를 보겠습니다.

```bash
./sqlproc_demo case_basic_users_reset.txt
./sqlproc_demo case_basic_users_run.txt
```

여기서는 전체 조회 후 새 사용자를 INSERT하고, 다시 조회해서 들어갔는지 확인합니다.  
그다음 UPDATE로 이름을 바꾸고, 마지막으로 DELETE 후 다시 SELECT해서 원래 상태로 돌아온 것을 확인합니다.  
즉, 한 번의 시나리오 안에서 조회, 삽입, 수정, 삭제가 모두 검증됩니다.

### 2:05 - 3:00

다음은 제약조건 테스트입니다.

```bash
./sqlproc_demo case_constraints_users_reset.txt
./sqlproc_demo case_constraints_users_run.txt
```

이 테스트에서는 PK 중복, UK 중복, NN 위반을 각각 넣어 봅니다.  
실행 결과를 보면 잘못된 데이터는 에러 메시지와 함께 차단되고, 정상 데이터만 반영됩니다.  
이 부분이 중요한 이유는, SQL을 읽는 것에서 끝나는 게 아니라 실제 데이터 무결성까지 검증한다는 점입니다.

### 3:00 - 3:20

추가로 quoted 값과 잘못된 SQL 문장도 케이스 파일로 검증했습니다.  
예를 들어 이메일 안에 쉼표가 있어도 파싱이 깨지지 않고, 문법이 틀린 SQL은 즉시 거부됩니다.

### 3:20 - 3:30

정리하면 저희 프로젝트는 SQL 문장을 읽고, 구조화하고, 실제 파일 데이터에 반영하는 작은 DB 처리기입니다.  
빠르게 구현하는 데서 끝나지 않고, 테스트 케이스로 정상 흐름과 예외 흐름까지 검증한 점이 핵심입니다.

## 시간 초과 방지 팁

- 빌드는 발표 전에 끝내고 들어갑니다.
- 시연은 `basic`과 `constraints` 두 케이스만 실행합니다.
- 질문이 나오면 `quotes`와 `invalid` 케이스를 백업 자료로 보여줍니다.
