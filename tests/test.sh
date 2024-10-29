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
        --print-ir)
            MODE="print"
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

    rm -f out.ll
}

run_print() {
    FILE_PATH="${1%.charon}"
    FILE_NAME="${TEST_PATH##*/}"

    if ! [ -f "$1" ]; then
        print_result 0 $TEST_NAME "Missing $FILE_NAME.charon file"
        return
    fi

    ./tests/runners/full $1 out.ll
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $FILE_NAME "tester exited with $EXIT_CODE"
        return
    fi

    echo -e "\e[36m"
    cat out.ll
    echo -e "\e[0m"

    print_result 1 $FILE_NAME

    lli out.ll

    rm -f out.ll
}

run_all() {
    make clean tests/runners/parse
    echo "| Running Parse Tests"
    for TEST_FILE in tests/parse/*.test; do run_parse $TEST_FILE; done
    echo "| Done"

    make tests/runners/full
    echo "| Running Execution Tests"
    for TEST_FILE in tests/exec/*.test; do run_exec $TEST_FILE; done
    echo "| Done"
    make clean
}

case $MODE in
    all)
        make clean tests/runners/parse tests/runners/full

        echo "| Running Parse Tests"
        for TEST_FILE in tests/parse/*.test; do run_parse $TEST_FILE; done
        echo "| Done"

        echo "| Running Execution Tests"
        for TEST_FILE in tests/exec/*.test; do run_exec $TEST_FILE; done
        echo "| Done"

        make clean
        ;;
    parse)
        make clean tests/runners/parse

        echo "| Running Parse Test \`$1\`"
        run_parse tests/parse/$1.test
        echo "| Done"

        make clean
        ;;
    exec)
        make clean tests/runners/full

        echo "| Running Execution Test \`$1\`"
        run_exec tests/exec/$1.test
        echo "| Done"

        make clean
        ;;
    print)
        make clean tests/runners/full

        echo "| Running Print \`$1\`"
        run_print tests/$1.charon
        echo "| Done"

        make clean
        ;;
esac