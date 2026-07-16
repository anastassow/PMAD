CC       ?= gcc
CSTD     := -std=c11
WARN     := -Wall -Wextra
CPPFLAGS := -Iinclude
CFLAGS   ?= -g -O0
LDFLAGS  :=

BUILD_DIR := build
BIN       := main

SRCS := main.c $(wildcard src/*.c)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all run debug release clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(WARN) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./$(BIN)

debug: CFLAGS := -g -O0 -DDEBUG
debug: clean all

release: CFLAGS := -O2 -DNDEBUG
release: clean all

clean:
	rm -rf $(BUILD_DIR) $(BIN)

-include $(DEPS)
