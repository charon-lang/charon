.PHONY: all clean

all: clean

CC := clang
CFLAGS := -std=gnu23 -D PCRE2_CODE_UNIT_WIDTH=8
CFLAGS += -Werror -Wswitch -Wimplicit-fallthrough -Wall
CFLAGS += $(shell pcre2-config --cflags --libs8)
CFLAGS += $(shell llvm-config --cflags --ldflags --system-libs --libs core)
C_SOURCES := $(shell find src -type f -name "*.c")

tests/runners/%: tests/runners/%.c
	@ $(CC) -I./src $(CFLAGS) -g -o $@ $< $(C_SOURCES)

clean:
	@ rm -f $(shell find tests/runners/* ! -name "*.c")