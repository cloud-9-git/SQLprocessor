#ifndef SQLPROC_VALUE_H
#define SQLPROC_VALUE_H

#include "sqlproc/diag.h"

/* 현재 엔진이 직접 이해할 수 있는 기본 타입입니다. */
typedef enum {
    DATA_TYPE_INT = 0,
    DATA_TYPE_TEXT = 1,
    DATA_TYPE_BOOL = 2
} DataType;

/* 하나의 셀 값을 표현하는 공용 구조체입니다.
 * type 필드가 현재 union 안에서 어떤 값이 유효한지 결정합니다.
 */
typedef struct {
    DataType type;
    union {
        long long int_value;
        int bool_value;
        char *text_value;
    } as;
} Value;

/* Value를 안전한 기본값으로 초기화합니다. */
void value_init(Value *value);

/* 정수 값을 담는 Value를 생성합니다. */
Value value_make_int(long long value);

/* 불리언 값을 담는 Value를 생성합니다. */
Value value_make_bool(int value);

/* 문자열을 복사해 TEXT Value를 생성합니다. */
SqlStatus value_make_text(Value *out, const char *text, SqlError *err);

/* Value를 깊은 복사합니다. TEXT는 문자열까지 새로 복사합니다. */
SqlStatus value_clone(const Value *src, Value *dest, SqlError *err);

/* Value가 들고 있는 동적 메모리를 해제합니다. */
void value_free(Value *value);

/* 두 Value가 같은 타입/같은 값인지 비교합니다. */
int value_equal(const Value *left, const Value *right);

/* DataType enum을 `INT`, `TEXT`, `BOOL` 문자열로 바꿉니다. */
const char *value_type_name(DataType type);

/* 스키마 파일의 타입 문자열을 DataType으로 해석합니다. */
SqlStatus value_parse_type_name(const char *name, DataType *out_type);

/* Value를 저장/출력용 평문 문자열로 바꿉니다. */
SqlStatus value_to_plain_text(const Value *value, char **out_text, SqlError *err);

#endif
