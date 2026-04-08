CC = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2
TARGET = sql_processor
SRC = src/main.c src/common.c src/parser.c src/storage.c src/processor.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf tests/tmp-db

test: $(TARGET)
	sh tests/test_roundtrip.sh
