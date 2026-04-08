-- demo_edge_cases.sql의 발표용 별칭 파일
-- 조회 결과 없음, quoted 값, 제약 위반, 대상 없는 수정/삭제 등을 한 번에 확인합니다.

-- 0. 시작 전에 기준 데이터로 초기화
DELETE FROM case_basic_users WHERE id = 1;
DELETE FROM case_basic_users WHERE id = 2;
DELETE FROM case_basic_users WHERE id = 3;
DELETE FROM case_basic_users WHERE id = 4;
DELETE FROM case_basic_users WHERE id = 5;

INSERT INTO case_basic_users VALUES (1, 'admin@test.com', '010-1111', 'pass123', 'Admin');
INSERT INTO case_basic_users VALUES (2, 'user1@test.com', '010-2222', 'qwerty', 'UserOne');
INSERT INTO case_basic_users VALUES (3, 'user2@test.com', '010-3333', 'hello123', 'UserTwo');
SELECT * FROM case_basic_users;

-- 1. 조회 결과가 없는 경우
SELECT * FROM case_basic_users WHERE id = 999;

-- 2. 문자열 안의 쉼표와 세미콜론 처리
INSERT INTO case_basic_users VALUES (4, 'edge@test.com', '010-4444', 'pw;4444', 'Kim, Jr');
SELECT * FROM case_basic_users WHERE id = 4;
SELECT * FROM case_basic_users WHERE name = 'Kim, Jr';

-- 3. PK 중복 INSERT 차단
INSERT INTO case_basic_users VALUES (1, 'dup-pk@test.com', '010-9999', 'dup123', 'DupPk');
SELECT * FROM case_basic_users WHERE id = 1;

-- 4. UK(email) 중복 INSERT 차단
INSERT INTO case_basic_users VALUES (5, 'admin@test.com', '010-5555', 'pw5555', 'DupEmail');

-- 5. UK(phone) 중복 INSERT 차단
INSERT INTO case_basic_users VALUES (5, 'unique@test.com', '010-1111', 'pw5555', 'DupPhone');

-- 6. NN(pwd) 빈 값 INSERT 차단
INSERT INTO case_basic_users VALUES (5, 'nn@test.com', '010-5555', '', 'NoPassword');

-- 7. PK 컬럼 UPDATE 차단
UPDATE case_basic_users SET id = 10 WHERE id = 2;
SELECT * FROM case_basic_users WHERE id = 2;

-- 8. UK(email) 중복 UPDATE 차단
UPDATE case_basic_users SET email = 'admin@test.com' WHERE id = 2;
SELECT * FROM case_basic_users WHERE id = 2;

-- 9. NN(pwd) 빈 값 UPDATE 차단
UPDATE case_basic_users SET pwd = '' WHERE id = 2;
SELECT * FROM case_basic_users WHERE id = 2;

-- 10. UPDATE 대상이 없는 경우
UPDATE case_basic_users SET name = 'Nobody' WHERE id = 999;

-- 11. DELETE 대상이 없는 경우
DELETE FROM case_basic_users WHERE id = 999;

-- 12. 테스트 중 추가한 행 정리 후 최종 상태 확인
DELETE FROM case_basic_users WHERE id = 4;
SELECT * FROM case_basic_users;
