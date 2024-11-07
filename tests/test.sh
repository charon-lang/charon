#!/usr/bin/env sh

MODE="all"

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            MODE="all"
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

run_exec() {
    TEST_PATH="${1%.test}"
    TEST_NAME="${TEST_PATH##*/}"

    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        return
    fi

    ./charon --llvm-ir $CHARON_FILE
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester exited with $EXIT_CODE"
        return
    fi

    RESULT="$(lli $TEST_PATH.ll)"
    EXPECTED="$(cat $1)"
    if [[ "$RESULT" == "$EXPECTED" ]]; then
        print_result 1 $TEST_NAME
    else
        print_result 0 $TEST_NAME
        diff --color <(echo "$RESULT") <(echo "$EXPECTED")
    fi

    rm -f $TEST_PATH.ll
}

make clean charon

case $MODE in
    all)
        echo "| Running Execution Tests"
        for TEST_FILE in tests/exec/*.test; do run_exec $TEST_FILE; done
        echo "| Done"
        ;;
esac