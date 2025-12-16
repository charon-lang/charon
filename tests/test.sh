#!/usr/bin/env bash

MODE="all"
PASSED=0
FAILED=0
TOTAL=0

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            MODE="all"
            shift
            ;;
        -p|--parse)
            MODE="parse"
            shift
            ;;
        -*|--*)
            echo "Unknown option $1"
            exit 1
            ;;
        *)
            POSITIONAL_ARGS+=("$1")
            shift
            ;;
    esac
done

set -- "${POSITIONAL_ARGS[@]}"

print_result() {
    echo -n "| "
    if [ $1 -eq 0 ]; then
        echo -ne "\e[41;30mFAIL"
    else
        echo -ne "\e[42;30mPASS"
    fi
    echo -ne "\e[0m"
    echo -n " $2"
    if [ $# -eq 3 ]; then
        echo -n " ($3)"
    fi
    echo -ne "\n"
}

run_parse_test() {
    TEST_PATH="${1%.test}"
    TEST_NAME="${TEST_PATH##*/}"
    TOTAL=$(($TOTAL+1))
    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        return
    fi

    ./build/compiler/charonc $CHARON_FILE --raw-output --no-sema > $TEST_PATH.result
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester exited with $EXIT_CODE"
        return
    fi

    RESULT="$(cat $TEST_PATH.result)"
    EXPECTED="$(cat $1)"
    if [[ "$RESULT" == "$EXPECTED" ]]; then
        print_result 1 $TEST_NAME
        PASSED=$(($PASSED+1))
        rm -f $TEST_PATH.result
    else
        print_result 0 $TEST_NAME
        FAILED=$(($FAILED+1))
        diff --color <(echo "$RESULT") <(echo "$EXPECTED")
    fi
}

run_parse_tests() {
    PASSED=0
    FAILED=0
    TOTAL=0

    echo "| Running Parse Tests"
    for TEST_FILE in tests/parse/*.test; do run_parse_test $TEST_FILE; done
    echo "| Done $PASSED/$TOTAL Passed ($FAILED Failed)"
}

case $MODE in
    all)
        run_parse_tests
        ;;
    parse)
        run_parse_tests
        ;;
esac
