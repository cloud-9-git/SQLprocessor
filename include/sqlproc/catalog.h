#ifndef SQLPROC_CATALOG_H
#define SQLPROC_CATALOG_H

#include "sqlproc/schema.h"

typedef struct {
    const char *db_root;
} Catalog;

void catalog_init(Catalog *catalog, const char *db_root);
SqlStatus catalog_load_table(const char *db_root, const char *table_name, TableSchema *out_schema, SqlError *err);

#endif
