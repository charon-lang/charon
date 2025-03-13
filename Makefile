.PHONY: all clean

all: clean charon

CC := clang
CFLAGS := -xc -std=gnu23 -D PCRE2_CODE_UNIT_WIDTH=8
CFLAGS += -Werror -Wswitch -Wimplicit-fallthrough -Wall
CFLAGS += $(shell pcre2-config --cflags --libs8)
CFLAGS += $(shell llvm-config --cflags --ldflags --system-libs --libs core)
C_SOURCES := $(shell find src -type f -name "*.c")

ifeq ($(ENVIRONMENT), production)
CFLAGS += -s -O3
else
CFLAGS += -g -fsanitize=undefined
endif

charon:
	$(CC) -I./src $(CFLAGS) -o $@ $(C_SOURCES)

clean:
	@ rm -f charon
