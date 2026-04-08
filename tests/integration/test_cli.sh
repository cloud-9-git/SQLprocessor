#!/bin/sh
set -eu

SQLPROC_BIN="${SQLPROC_BIN:-build/sqlproc}"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/sqlproc-it-XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/db"
cp -R tests/fixtures/db/schema "$TMP_DIR/db/schema"
cp -R tests/fixtures/db/data "$TMP_DIR/db/data"

"$SQLPROC_BIN" --db-root "$TMP_DIR/db" --input tests/fixtures/sql/select_active.sql >"$TMP_DIR/select.out"
cmp -s "$TMP_DIR/select.out" tests/fixtures/expected/select_active.tsv

"$SQLPROC_BIN" --db-root "$TMP_DIR/db" --input tests/fixtures/sql/mixed.sql >"$TMP_DIR/mixed.out"
cmp -s "$TMP_DIR/mixed.out" tests/fixtures/expected/mixed.tsv
grep -q "Dana" "$TMP_DIR/db/data/users.rows"

cp "$TMP_DIR/db/data/users.rows" "$TMP_DIR/before.rows"
"$SQLPROC_BIN" --db-root "$TMP_DIR/db" --input tests/fixtures/sql/check_only.sql --check >"$TMP_DIR/check.out"
cmp -s "$TMP_DIR/db/data/users.rows" "$TMP_DIR/before.rows"
cmp -s "$TMP_DIR/check.out" tests/fixtures/expected/check_ok.txt

if "$SQLPROC_BIN" --db-root "$TMP_DIR/db" --input tests/fixtures/sql/invalid_type.sql >"$TMP_DIR/invalid.out" 2>"$TMP_DIR/invalid.err"; then
    echo "expected invalid_type.sql to fail" >&2
    exit 1
fi
grep -q "type mismatch" "$TMP_DIR/invalid.err"

echo "integration tests passed"
