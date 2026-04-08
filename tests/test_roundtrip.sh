#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TMP_DB="$ROOT_DIR/tests/tmp-db"
OUTPUT_FILE="$ROOT_DIR/tests/roundtrip.out"

rm -rf "$TMP_DB"
mkdir -p "$TMP_DB"

"$ROOT_DIR/sql_processor" "$ROOT_DIR/tests/roundtrip.sql" "$TMP_DB" > "$OUTPUT_FILE"
diff -u "$ROOT_DIR/tests/roundtrip.expected" "$OUTPUT_FILE"

rm -f "$OUTPUT_FILE"
rm -rf "$TMP_DB"
