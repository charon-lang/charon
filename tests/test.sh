#!/usr/bin/env sh

MODE="all"

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            MODE="all"
            shift
            ;;
        -s|--single)
            MODE="single"
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

run_test_parse() {
    TEST_PATH="${1%.test}"
    TEST_NAME="${TEST_PATH##*/}"

    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        return
    fi

    RESULT="$(./tests/runners/parse $CHARON_FILE $(sed -n -e 1p $1))"
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester exited with $EXIT_CODE"
        return
    fi

    EXPECTED="$(tail +2 $1)"
    if [[ "$RESULT" == "$EXPECTED" ]]; then
        print_result 1 $TEST_NAME
    else
        print_result 0 $TEST_NAME
        diff --color <(echo "$RESULT") <(echo "$EXPECTED")
    fi
}

case $MODE in
    single)
        make clean tests/runners/parse
        run_test_parse tests/parse/$1.test
        ;;
    all)
        make clean tests/runners/parse
        echo "| Running All Tests"
        for TEST_FILE in tests/parse/*.test; do
            echo -n "| "
            run_test_parse $TEST_FILE
        done
        echo "| Done"
        ;;
esac