#ifndef SQLPROC_VALUE_H
#define SQLPROC_VALUE_H

#include "sqlproc/diag.h"

typedef enum {
    DATA_TYPE_INT = 0,
    DATA_TYPE_TEXT = 1,
    DATA_TYPE_BOOL = 2
} DataType;

typedef struct {
    DataType type;
    union {
        long long int_value;
        int bool_value;
        char *text_value;
    } as;
} Value;

void value_init(Value *value);
Value value_make_int(long long value);
Value value_make_bool(int value);
SqlStatus value_make_text(Value *out, const char *text, SqlError *err);
SqlStatus value_clone(const Value *src, Value *dest, SqlError *err);
void value_free(Value *value);
int value_equal(const Value *left, const Value *right);
const char *value_type_name(DataType type);
SqlStatus value_parse_type_name(const char *name, DataType *out_type);
SqlStatus value_to_plain_text(const Value *value, char **out_text, SqlError *err);

#endif
