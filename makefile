CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g

TARGET = test_connection
OBJ = connection.o test_connection.o 

all: $(TARGET)

$(TARGET): $(OBJ)
$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

connection.o: connection.c connection.h constants.h
$(CC) $(CFLAGS) -c connection.c -o connection.o 

test_connection.o: test_connection.c connection.h
$(CC) $(CFLAGS) -c test_connection.c -o test_connection.o 

runL $(TARGET)
./$(TARGET)

clean:
rm -f $(OBJ) $(TARGET)

.PHONY: all run clean