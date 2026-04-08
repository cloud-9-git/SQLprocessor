INSERT INTO demo.users (id, name, team) VALUES (1, 'Kim', 'platform');
INSERT INTO demo.users (id, name, team) VALUES (2, 'Lee', 'data');
INSERT INTO demo.users VALUES (3, 'Park', 'infra');
SELECT * FROM demo.users;
SELECT name, team FROM demo.users;
SELECT name FROM demo.users WHERE team = 'data';
SELECT id, name FROM demo.users WHERE id = 3;
