.PHONY: run

C_SOURCES := $(shell find ../src -type f -name "*.c" -not -name "charon.c")

run:
	@ gcc -I../src $(CFLAGS) -o parse_tester parse_tester.c $(C_SOURCES)
	@ ./run_tests.sh
	@ rm -f parse_tester