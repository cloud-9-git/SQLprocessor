CC ?= cc
CFLAGS_COMMON := -std=c11 -Wall -Wextra -Werror -pedantic -D_POSIX_C_SOURCE=200809L
CFLAGS_DEBUG := -g -O0
CFLAGS_RELEASE := -O2
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
INCLUDES := -Iinclude

SRC := \
	src/main.c \
	src/diag.c \
	src/value.c \
	src/lexer.c \
	src/parser.c \
	src/binder.c \
	src/planner.c \
	src/catalog.c \
	src/storage.c \
	src/executor.c \
	src/renderer.c

ENGINE_SRC := \
	src/diag.c \
	src/value.c \
	src/lexer.c \
	src/parser.c \
	src/binder.c \
	src/planner.c \
	src/catalog.c \
	src/storage.c \
	src/executor.c \
	src/renderer.c

TEST_SRC := \
	tests/unit/test_runner.c \
	tests/unit/test_lexer.c \
	tests/unit/test_parser.c \
	tests/unit/test_binder.c \
	tests/unit/test_planner.c \
	tests/unit/test_storage.c \
	tests/unit/test_executor.c

.PHONY: debug release test test-sanitize clean

debug: build/sqlproc

release: build/sqlproc-release

test: build/test_runner build/sqlproc
	build/test_runner
	sh tests/integration/test_cli.sh

test-sanitize: build/test_runner-sanitize build/sqlproc-sanitize
	build/test_runner-sanitize
	SQLPROC_BIN=build/sqlproc-sanitize sh tests/integration/test_cli.sh

clean:
	rm -rf build

build:
	mkdir -p build

build/sqlproc: $(SRC) | build
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(INCLUDES) $(SRC) -o $@

build/sqlproc-release: $(SRC) | build
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_RELEASE) $(INCLUDES) $(SRC) -o $@

build/sqlproc-sanitize: $(SRC) | build
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(SAN_FLAGS) $(INCLUDES) $(SRC) -o $@

build/test_runner: $(ENGINE_SRC) $(TEST_SRC) | build
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(INCLUDES) $(ENGINE_SRC) $(TEST_SRC) -o $@

build/test_runner-sanitize: $(ENGINE_SRC) $(TEST_SRC) | build
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(SAN_FLAGS) $(INCLUDES) $(ENGINE_SRC) $(TEST_SRC) -o $@
