#!/usr/bin/env sh

MODE="all"

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            MODE="all"
            shift
            ;;
        -e|--exec)
            MODE="exec"
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

run_parse() {
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

run_exec() {
    TEST_PATH="${1%.test}"
    TEST_NAME="${TEST_PATH##*/}"

    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        return
    fi

    ./tests/runners/full $CHARON_FILE out.ll
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester exited with $EXIT_CODE"
        return
    fi

    RESULT="$(lli out.ll)"
    EXPECTED="$(cat $1)"
    if [[ "$RESULT" == "$EXPECTED" ]]; then
        print_result 1 $TEST_NAME
    else
        print_result 0 $TEST_NAME
        diff --color <(echo "$RESULT") <(echo "$EXPECTED")
    fi
}

run_all_parse() {
    make clean tests/runners/parse
    echo "| Running Parse Tests"
    for TEST_FILE in tests/parse/*.test; do run_parse $TEST_FILE; done
    echo "| Done"
}

run_all_exec() {
    make clean tests/runners/full
    echo "| Running Execution Tests"
    for TEST_FILE in tests/exec/*.test; do run_exec $TEST_FILE; done
    echo "| Done"
}

case $MODE in
    parse) run_all_parse ;;
    exec) run_all_exec;;
    all)
        run_all_parse
        run_all_exec
        ;;
esac