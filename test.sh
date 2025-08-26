#!/bin/bash

# if there is any error we will terminate the script
set -e

#define consts
ANALYZER="output/analyzer"
PASSED=0
FAILED=0
TOTAL=0


# test helper functions
run_test() {
    TOTAL=$((TOTAL + 1))
    echo -n "Test $TOTAL: $1 ... "
}

test_pass() {
    echo "PASS"
    PASSED=$((PASSED + 1))
}

test_fail() {
    echo "FAIL ($1)"
    FAILED=$((FAILED + 1))
}

echo "Starting To Test MY Modular Pipeline System"
echo "=========================================="

# Build first
echo "Building project..."
./build.sh

## check if build succeeded
# Test 1: checking if analyzer exist (the main core of the system) 
run_test "Analyzer executable exists - make sure build succeeded"
if [[ -x "$ANALYZER" ]]; then
    test_pass
else
    test_fail "analyzer not found"
fi

# Test 2: Checking if all the plugins exist
run_test "Plugin libraries exist"
missing=""
for plugin in logger uppercaser rotator flipper expander typewriter; do
    if [[ ! -f "output/${plugin}.so" ]]; then
        missing="$missing $plugin"
    fi
done
if [[ -z "$missing" ]]; then
    test_pass
else
    test_fail "missing:$missing"
fi

### args validations ###

# Test 3: No arguments ->should fail
run_test "No arguments - should reject"
if timeout 3s "$ANALYZER" >/dev/null 2>&1; then
    test_fail "should reject no args"
else
    test_pass
fi

# Test 4: only queue size without plugins- should fail
run_test "Run without plugins - should reject"
if timeout 3s "$ANALYZER" 10 >/dev/null 2>&1; then
    test_fail "should reject missing plugins"
else
    test_pass
fi

# Test 5: queue size - 0 negative or lagre - should fail
run_test "Invalid queue size (0, neg and large) - should reject them all"
if timeout 3s "$ANALYZER" 0 logger >/dev/null 2>&1 && \
   timeout 3s "$ANALYZER" -5 logger >/dev/null 2>&1 && \
   timeout 3s "$ANALYZER" 2000000 logger >/dev/null 2>&1; then
    test_fail "invalid queue sizes (one of them - zero, negative, or large number)"
else
    test_pass "All invalid queue sizes properly rejected"
fi



# Test 6: if we run on plugin which does not exist - should fail
run_test "Non-existent plugin rejection"
if timeout 3s "$ANALYZER" 10 badplugin >/dev/null 2>&1; then
    test_fail "should reject bad plugin"
else
    test_pass
fi

## check plugin behavior ###

# Test 7: logger
run_test " logger plugin"
result=$(echo -e "hello\n<END>" | timeout 5s "$ANALYZER" 10 logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] hello" ]]; then

    test_pass
else
     test_fail "expected '[logger] hello', got '$result'"
fi

# Test 8: Uppercaser 
run_test "Uppercaser "
result=$(echo -e "test\n<END>" | timeout 5s "$ANALYZER" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] TEST" ]]; then
    test_pass
else
    test_fail "expected '[logger] TEST', got '$result'"
fi

# Test 9: Rotator 
run_test "Rotator "
result=$(echo -e "abc\n<END>" | timeout 5s "$ANALYZER" 10 rotator logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] cab" ]]; then
    test_pass
else
    test_fail "expected '[logger] cab', got '$result'"
fi

# Test 10: Flipper   
run_test "Flipper plugin"
result=$(echo -e "hello\n<END>" | timeout 5s "$ANALYZER" 10 flipper logger 2>/dev/null | grep "\[logger\]" || true)

if [[ "$result" == "[logger] olleh" ]]; then
    test_pass

else
    test_fail "expected '[logger] olleh', got '$result'"
fi

# Test 11: Expander 
run_test "Expander "

result=$(echo -e "hi\n<END>" | timeout 5s "$ANALYZER" 10 expander logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] h i" ]]; then

    test_pass
else
    test_fail "expected '[logger] h i', got '$result'"
fi

# Test 12: three plugins without repeated
run_test "Pipeline chain: uppercaser -> rotator ->logger"

result=$(echo -e "abc\n<END>" | timeout 5s "$ANALYZER" 10 uppercaser rotator logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] CAB" ]]; then
    test_pass
else

    test_fail "expected '[logger] CAB', got '$result'"
fi

# Test 13: another check some different plugins
run_test "Pipeline chain: uppercaser -> rotator -> flipper -> logger"

result=$(echo -e "abc\n<END>" | timeout 5s "$ANALYZER" 10 uppercaser rotator flipper logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] BAC" ]]; then
    test_pass
else

    test_fail "expected '[logger] BAC', got '$result'"
fi

# Test 14: Repeated plugin
run_test "use the same plugin twice: rotator -> rotator -> logger"
result=$(echo -e "abc\n<END>" | timeout 5s "$ANALYZER" 10 rotator rotator logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] bca" ]]; then

    test_pass
else
    test_fail "expected '[logger] bca', got '$result'"
fi

# Test 15: Empty input 
run_test "Empty input"
result=$(echo -e "\n<END>" | timeout 5s "$ANALYZER" 10 logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] " ]]; then
    test_pass
else
    test_fail "expected '[logger] ', got '$result'"
fi

# Test 16: one char
run_test "One character"
result=$(echo -e "a\n<END>" | timeout 5s "$ANALYZER" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] A" ]]; then
    test_pass

else
    test_fail "expected '[logger] A', got '$result'"

fi



# Test 18: complex string with numbers
run_test "string and numbers input"
result=$(echo -e "123abc\n<END>" | timeout 5s "$ANALYZER" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
if [[ "$result" == "[logger] 123ABC" ]]; then
    test_pass
else
    test_fail "expected '[logger] 123ABC', got '$result'"
fi


# Test 19: if we had <END> so we should make and get graceful shutdown message
run_test "with <END> - Graceful shutdown message"
output=$(echo -e "test\n<END>" | timeout 5s "$ANALYZER" 10 logger 2>/dev/null)
if [[ "$output" == *"Pipeline shutdown complete"* ]]; then
    test_pass
else
    test_fail "missing shutdown message"
fi


# Test 20: input lines with \n 
run_test "input lines with several \\n "

lines="line1\nline2\nline3\n<END>"
count=$(echo -e "$lines" | timeout 10s "$ANALYZER" 20 uppercaser flipper logger 2>/dev/null | grep "\[logger\]" | wc -l)
if [[ "$count" -eq 3 ]]; then
    test_pass
else
    test_fail "expected 3 lines, got $count"
fi


# Test 21: Long string
run_test "Long string processing"
long_string=$(printf 'a%.0s' {1..100000})
if echo -e "${long_string}\n<END>" | timeout 10s "$ANALYZER" 20 logger >/dev/null 2>&1; then
    test_pass
else
    test_fail "failed processing long string"
fi


# Test 22: All plugins pipeline
run_test "All plugins together"
if echo -e "test\n<END>" | timeout 15s "$ANALYZER" 30 uppercaser rotator flipper expander logger >/dev/null 2>&1; then
    test_pass
else
    test_fail "all plugins pipeline failed"
fi



# summerize tests results 
echo ""
echo "===================================="
echo "Test Results:"
echo "Passed: $PASSED" 
echo "Failed: $FAILED"
echo "Total:  $TOTAL"


if [[ $FAILED -eq 0 ]]; then
    echo "You are the best!! All tests passed! you can go to the beach now! 🎉"
    exit 0
else  
    pass_rate=$((PASSED * 100 / TOTAL))
    echo "Some tests failed (${pass_rate}% pass rate)"
    exit 1
fi