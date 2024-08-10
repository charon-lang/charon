.PHONY: all clean

all: clean
	clang -std=gnu23 -Wswitch -Wimplicit-fallthrough -Wall $(shell llvm-config --cflags --ldflags --system-libs --libs core) -o testbench main.c
	./testbench

clean:
	rm -f testbench main.ll