.PHONY: all clean run-test-%

all: clean build/charon

CFLAGS := -std=gnu23 -D PCRE2_CODE_UNIT_WIDTH=8
CFLAGS += -Werror -Wswitch -Wimplicit-fallthrough -Wall
CFLAGS += $(shell pcre2-config --cflags --libs8)
CFLAGS += $(shell llvm-config --cflags --ldflags --system-libs --libs core)
C_SOURCES := $(shell find src -type f -name "*.c")

build/charon:
	@ mkdir -p $(@D)
	gcc -I./src $(CFLAGS) -o $@ $(C_SOURCES)

clean:
	rm -rf ./build

run-test-%:
	@ echo -e "\n-- Compiling test $(*)"
	build/charon -o build/test.ll tests/$(*).charon
	@ echo -e "\n-- Running test $(*)"
	@ lli build/test.ll