-- 데모 시작 전 users 테이블을 기준 상태로 되돌립니다.
DELETE FROM case_basic_users WHERE id = 1;
DELETE FROM case_basic_users WHERE id = 2;
DELETE FROM case_basic_users WHERE id = 3;
DELETE FROM case_basic_users WHERE id = 4;
DELETE FROM case_basic_users WHERE id = 5;

INSERT INTO case_basic_users VALUES (1, 'admin@test.com', '010-1111', 'pass123', 'Admin');
INSERT INTO case_basic_users VALUES (2, 'user1@test.com', '010-2222', 'qwerty', 'UserOne');
INSERT INTO case_basic_users VALUES (3, 'user2@test.com', '010-3333', 'hello123', 'UserTwo');
