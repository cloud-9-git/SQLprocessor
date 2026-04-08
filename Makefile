CC = gcc
CFLAGS = -fdiagnostics-color=always -g
TARGET = sqlsprocessor
SRC = main.c
SQL_FILE ?= demo_select.sql

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) "$(SQL_FILE)"

clean:
	rm -f $(TARGET)
