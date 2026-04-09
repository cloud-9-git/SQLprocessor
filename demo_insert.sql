-- INSERT 기능만 시연합니다.
INSERT INTO case_basic_users VALUES (4, 'newuser@test.com', '010-4444', 'pw4444', 'NewUser');
SELECT * FROM case_basic_users WHERE id = 4;
