.PHONY: run run-parse run-full

C_SOURCES := $(shell find ../src -type f -name "*.c" -not -name "charon.c")

run-parse:
	@ gcc -I../src $(CFLAGS) -o tester_parse tester_parse.c $(C_SOURCES)
	@ ./run_parse_tests.sh
	@ rm -f tester_parse

run-full:
	@ rm -f out.ll
	@ gcc -I../src $(CFLAGS) -o tester_full tester_full.c $(C_SOURCES)
	@ ./tester_full full/test.charon out.ll
	@ rm -f tester_full
	@ lli out.ll

run: run-parse run-full