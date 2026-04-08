#ifndef SQLPROC_CATALOG_H
#define SQLPROC_CATALOG_H

#include "sqlproc/schema.h"

/* 현재 작업 중인 DB 루트 경로를 들고 있는 가벼운 컨텍스트입니다. */
typedef struct {
    const char *db_root;
} Catalog;

/* Catalog에 DB 루트 경로를 연결합니다. */
void catalog_init(Catalog *catalog, const char *db_root);

/* schema/<table>.schema 파일을 읽어 TableSchema를 구성합니다. */
SqlStatus catalog_load_table(const char *db_root, const char *table_name, TableSchema *out_schema, SqlError *err);

#endif
