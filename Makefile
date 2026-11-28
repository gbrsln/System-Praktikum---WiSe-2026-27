CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -g $(EXTRA_CFLAGS)
DEPENDENCY_FLAGS := -MMD -MP

TARGET := sysprak-client
BUILD_DIR := build
# Dateien test_*.c haben eine eigene main() und gehören nicht in den Client.
SOURCES := $(filter-out test_%.c,$(wildcard *.c))
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/%.o)

MOCK_PORT := 13570
UNIT_TEST_BINARY := $(BUILD_DIR)/test_args
MOCK_SERVER_BINARY := $(BUILD_DIR)/mock_server
MOCK_TEST_RUNNER := $(BUILD_DIR)/run_mock_tests
MOCK_SERVER_FILES := tests/mock_server.c tests/mock_server.h

.PHONY: all dev test mock mockserver mocktest clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEPENDENCY_FLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

# Wie "make", nur dass jede Warnung den Build abbricht. Nicht für die Abgabe verwenden.
dev:
	$(MAKE) clean
	$(MAKE) all EXTRA_CFLAGS=-Werror

test: $(UNIT_TEST_BINARY)
	./$(UNIT_TEST_BINARY)

$(UNIT_TEST_BINARY): tests/test_args.c $(BUILD_DIR)/args.o args.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/test_args.c $(BUILD_DIR)/args.o

# Client, der sich statt mit dem Gameserver mit dem lokalen Mock-Server verbindet.
mock:
	$(MAKE) TARGET=sysprak-client-mock BUILD_DIR=build-mock \
		EXTRA_CFLAGS='$(EXTRA_CFLAGS) -DHOSTNAME=\"127.0.0.1\" -DPORTNUMBER=$(MOCK_PORT)'

mockserver: $(MOCK_SERVER_BINARY)

mocktest: mock $(MOCK_TEST_RUNNER)
	./$(MOCK_TEST_RUNNER)

$(MOCK_SERVER_BINARY): tests/mock_server_main.c $(MOCK_SERVER_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMOCK_PORT=$(MOCK_PORT) -o $@ tests/mock_server_main.c tests/mock_server.c

$(MOCK_TEST_RUNNER): tests/run_mock_tests.c $(MOCK_SERVER_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMOCK_PORT=$(MOCK_PORT) -o $@ tests/run_mock_tests.c tests/mock_server.c

clean:
	rm -rf build build-mock sysprak-client sysprak-client-mock tests/logs

-include $(OBJECTS:.o=.d)
