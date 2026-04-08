CC ?= gcc
CFLAGS ?= -fdiagnostics-color=always -g
TARGET ?= sqlsprocessor
SRC = main.c
SQL ?= demo_select.sql

.PHONY: all build run demo-reset demo-select demo-insert demo-insert-error demo-update demo-delete clean

all: build

build: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) $(SQL)

demo-reset: $(TARGET)
	./$(TARGET) demo_reset.sql

demo-select: $(TARGET)
	./$(TARGET) demo_select.sql

demo-insert: $(TARGET)
	./$(TARGET) demo_insert.sql

demo-insert-error: $(TARGET)
	./$(TARGET) demo_insert_error.sql

demo-update: $(TARGET)
	./$(TARGET) demo_update.sql

demo-delete: $(TARGET)
	./$(TARGET) demo_delete.sql

clean:
	rm -f $(TARGET)
