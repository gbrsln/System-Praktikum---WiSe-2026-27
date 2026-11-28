CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Werror -g

TARGET = test_connection
MOCK_TARGET = test_connection_mock

MOCK_FLAGS = -DHOSTNAME='"127.0.0.1"' -DPORTNUMBER=13570

OBJ = connection.o test_connection.o
MOCK_OBJ = connection_mock.o test_connection.o

all: $(TARGET)

# Normaler Gameserver
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

connection.o: connection.c connection.h constants.h
	$(CC) $(CFLAGS) -c $< -o $@

test_connection.o: test_connection.c connection.h
	$(CC) $(CFLAGS) -c $< -o $@

# Mock-Server
mock: $(MOCK_TARGET)

$(MOCK_TARGET): $(MOCK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

connection_mock.o: connection.c connection.h constants.h
	$(CC) $(CFLAGS) $(MOCK_FLAGS) -c $< -o $@

# Programme ausführen
run: $(TARGET)
	./$(TARGET)

run-mock: $(MOCK_TARGET)
	./$(MOCK_TARGET)

# Generierte Dateien entfernen
clean:
	rm -f $(OBJ) $(MOCK_OBJ) $(TARGET) $(MOCK_TARGET)

.PHONY: all mock run run-mock clean
