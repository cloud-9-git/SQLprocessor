#include "sqlproc/value.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 문자열을 새 메모리에 복사하는 작은 유틸입니다. */
static char *dup_string(const char *text, SqlError *err) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* 타입 문자열 비교에서 대소문자를 무시하기 위한 헬퍼입니다. */
static int equals_ignore_case(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return 0;
        }
        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

/* Value를 안전한 기본값으로 초기화합니다. */
void value_init(Value *value) {
    if (value == NULL) {
        return;
    }

    value->type = DATA_TYPE_INT;
    value->as.int_value = 0;
}

/* 정수 Value를 즉시 생성합니다. */
Value value_make_int(long long value) {
    Value result;

    result.type = DATA_TYPE_INT;
    result.as.int_value = value;
    return result;
}

/* 불리언 Value를 즉시 생성합니다. */
Value value_make_bool(int value) {
    Value result;

    result.type = DATA_TYPE_BOOL;
    result.as.bool_value = value ? 1 : 0;
    return result;
}

/* TEXT Value를 만들 때 문자열까지 깊은 복사합니다. */
SqlStatus value_make_text(Value *out, const char *text, SqlError *err) {
    if (out == NULL) {
        sql_error_set(err, 0, 0, 0, "value output pointer is null");
        return SQL_STATUS_ERROR;
    }

    out->type = DATA_TYPE_TEXT;
    out->as.text_value = dup_string(text, err);
    if (text != NULL && out->as.text_value == NULL) {
        return SQL_STATUS_OOM;
    }

    return SQL_STATUS_OK;
}

/* Value 전체를 깊은 복사합니다. */
SqlStatus value_clone(const Value *src, Value *dest, SqlError *err) {
    if (src == NULL || dest == NULL) {
        sql_error_set(err, 0, 0, 0, "value clone received null pointer");
        return SQL_STATUS_ERROR;
    }

    dest->type = src->type;
    switch (src->type) {
        case DATA_TYPE_INT:
            dest->as.int_value = src->as.int_value;
            break;
        case DATA_TYPE_BOOL:
            dest->as.bool_value = src->as.bool_value;
            break;
        case DATA_TYPE_TEXT:
            dest->as.text_value = dup_string(src->as.text_value, err);
            if (src->as.text_value != NULL && dest->as.text_value == NULL) {
                return SQL_STATUS_OOM;
            }
            break;
        default:
            sql_error_set(err, 0, 0, 0, "unsupported value type");
            return SQL_STATUS_ERROR;
    }

    return SQL_STATUS_OK;
}

/* TEXT 타입이 들고 있는 메모리를 해제합니다. */
void value_free(Value *value) {
    if (value == NULL) {
        return;
    }

    if (value->type == DATA_TYPE_TEXT) {
        free(value->as.text_value);
        value->as.text_value = NULL;
    }
}

/* executor와 binder가 사용하는 공용 값 비교 함수입니다. */
int value_equal(const Value *left, const Value *right) {
    if (left == NULL || right == NULL || left->type != right->type) {
        return 0;
    }

    switch (left->type) {
        case DATA_TYPE_INT:
            return left->as.int_value == right->as.int_value;
        case DATA_TYPE_BOOL:
            return left->as.bool_value == right->as.bool_value;
        case DATA_TYPE_TEXT:
            if (left->as.text_value == NULL || right->as.text_value == NULL) {
                return left->as.text_value == right->as.text_value;
            }
            return strcmp(left->as.text_value, right->as.text_value) == 0;
        default:
            return 0;
    }
}

/* 타입 enum을 SQL/문서용 이름으로 바꿉니다. */
const char *value_type_name(DataType type) {
    switch (type) {
        case DATA_TYPE_INT:
            return "INT";
        case DATA_TYPE_TEXT:
            return "TEXT";
        case DATA_TYPE_BOOL:
            return "BOOL";
        default:
            return "UNKNOWN";
    }
}

/* 스키마 파일의 타입 문자열을 내부 enum으로 바꿉니다. */
SqlStatus value_parse_type_name(const char *name, DataType *out_type) {
    if (name == NULL || out_type == NULL) {
        return SQL_STATUS_ERROR;
    }

    if (equals_ignore_case(name, "INT")) {
        *out_type = DATA_TYPE_INT;
        return SQL_STATUS_OK;
    }
    if (equals_ignore_case(name, "TEXT")) {
        *out_type = DATA_TYPE_TEXT;
        return SQL_STATUS_OK;
    }
    if (equals_ignore_case(name, "BOOL")) {
        *out_type = DATA_TYPE_BOOL;
        return SQL_STATUS_OK;
    }

    return SQL_STATUS_ERROR;
}

/* Value를 저장 파일이나 표 출력에 사용할 평문 문자열로 만듭니다. */
SqlStatus value_to_plain_text(const Value *value, char **out_text, SqlError *err) {
    char buffer[64];
    int written;

    if (value == NULL || out_text == NULL) {
        sql_error_set(err, 0, 0, 0, "value_to_plain_text received null pointer");
        return SQL_STATUS_ERROR;
    }

    switch (value->type) {
        case DATA_TYPE_INT:
            written = snprintf(buffer, sizeof(buffer), "%lld", value->as.int_value);
            if (written < 0 || (size_t)written >= sizeof(buffer)) {
                sql_error_set(err, 0, 0, 0, "failed to format integer value");
                return SQL_STATUS_ERROR;
            }
            *out_text = dup_string(buffer, err);
            return *out_text == NULL ? SQL_STATUS_OOM : SQL_STATUS_OK;
        case DATA_TYPE_BOOL:
            *out_text = dup_string(value->as.bool_value ? "true" : "false", err);
            return *out_text == NULL ? SQL_STATUS_OOM : SQL_STATUS_OK;
        case DATA_TYPE_TEXT:
            *out_text = dup_string(value->as.text_value == NULL ? "" : value->as.text_value, err);
            return *out_text == NULL ? SQL_STATUS_OOM : SQL_STATUS_OK;
        default:
            sql_error_set(err, 0, 0, 0, "unsupported value type");
            return SQL_STATUS_ERROR;
    }
}
