# SQL Grammar

v1에서 지원하는 문법은 아래와 같습니다.

```ebnf
script        = { statement ";" } ;

statement     = insert_stmt | select_stmt ;

insert_stmt   = "INSERT" "INTO" identifier
                [ "(" identifier { "," identifier } ")" ]
                "VALUES"
                "(" literal { "," literal } ")" ;

select_stmt   = "SELECT" ( "*" | identifier { "," identifier } )
                "FROM" identifier
                [ "WHERE" identifier "=" literal ] ;

literal       = integer | string | boolean ;
boolean       = "true" | "false" ;
```

## Notes
- keyword는 대소문자를 구분하지 않습니다.
- string은 single quote를 사용합니다.
- escape는 `\n`, `\t`, `\\`, `\'` 를 지원합니다.
- `WHERE`는 현재 단일 equality predicate만 허용합니다.

## Planned Grammar Extensions
- `UPDATE ... SET ... WHERE ...`
- `DELETE FROM ... WHERE ...`
- `AND` / `OR`
- `LIMIT`
- `ORDER BY`
