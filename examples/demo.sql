-- 기본 INSERT
INSERT INTO academy.members (id, name, role) VALUES (1, 'Kim', 'backend');
INSERT INTO academy.members (id, name, role) VALUES (2, 'Lee', 'frontend');

-- 헤더가 생긴 뒤에는 컬럼 목록 없는 INSERT도 가능
INSERT INTO academy.members VALUES (3, 'Park', 'platform');

-- 전체 조회
SELECT * FROM academy.members;

-- 필요한 컬럼만 조회
SELECT name, role FROM academy.members;

-- 추가 구현: WHERE 조건 조회
SELECT id, name FROM academy.members WHERE role = 'frontend';
