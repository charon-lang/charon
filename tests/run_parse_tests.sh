#!/usr/bin/env sh

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

echo "| Running Tests"
for TEST_FILE in parse/*.test; do
    TEST_PATH="${TEST_FILE%.test}"
    TEST_NAME="${TEST_PATH##*/}"

    CHARON_FILE="$TEST_PATH.charon"
    if ! [ -f "$CHARON_FILE" ]; then
        print_result 0 $TEST_NAME "Missing $TEST_NAME.charon file"
        continue
    fi

    RESULT="$(./tester_parse $CHARON_FILE $(sed -n -e 1p $TEST_FILE))"
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        print_result 0 $TEST_NAME "tester_parse exited with $EXIT_CODE"
        continue
    fi

    EXPECTED="$(tail +2 $TEST_FILE)"

    if [[ "$RESULT" == "$EXPECTED" ]]; then
        print_result 1 $TEST_NAME
    else
        print_result 0 $TEST_NAME
        diff --color <(echo "$RESULT") <(echo "$EXPECTED")
    fi
done
echo "| Done"