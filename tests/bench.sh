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
    if [ "$MODE" = "bench" ]; then
        return
    fi
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
    TOTAL=$(($TOTAL+1))
    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        return
    fi

    ./build/compiler/charonc $CHARON_FILE --quiet
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester exited with $EXIT_CODE"
        return
    fi
}

run_test() {
    for i in $(seq 1 100); do
        run_exec "$1"
    done
}

case $MODE in
    all)
        for TEST_FILE in tests/exec/*.test; do run_test $TEST_FILE; done
        ;;
esac
