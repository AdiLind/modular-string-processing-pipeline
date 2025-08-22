#!/bin/bash

# Exit on any error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
OUTPUT_DIR="output"
ANALYZER="${OUTPUT_DIR}/analyzer"
TEST_OUTPUT_DIR="test_results"
PLUGINS=("logger" "uppercaser" "rotator" "flipper" "expander" "typewriter")
TIMEOUT_DURATION=10

# Test statistics
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Function to print colored output
print_test_header() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${BLUE}TEST CATEGORY: $1${NC}"
    echo -e "${CYAN}========================================${NC}"
}

print_test_case() {
    echo -e "\n${YELLOW}[TEST]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to run test with timeout
run_with_timeout() {
    local timeout_cmd=""
    if command -v timeout >/dev/null 2>&1; then
        timeout_cmd="timeout ${TIMEOUT_DURATION}"
    fi
    
    $timeout_cmd "$@"
    return $?
}

# Function to increment test counter
count_test() {
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
}

# Setup test environment
setup_tests() {
    print_info "Setting up test environment..."
    
    # Create test output directory
    mkdir -p "${TEST_OUTPUT_DIR}"
    
    # Clean any previous test files
    rm -f "${TEST_OUTPUT_DIR}"/*
    
    # Build the project first
    print_info "Building project..."
    if ! ./build.sh > "${TEST_OUTPUT_DIR}/build.log" 2>&1; then
        print_error "Build failed! Check ${TEST_OUTPUT_DIR}/build.log"
        exit 1
    fi
    
    # Verify main executable exists
    if [ ! -x "${ANALYZER}" ]; then
        print_error "Main executable not found: ${ANALYZER}"
        exit 1
    fi
    
    # Verify all plugins exist
    for plugin in "${PLUGINS[@]}"; do
        if [ ! -f "${OUTPUT_DIR}/${plugin}.so" ]; then
            print_error "Plugin not found: ${OUTPUT_DIR}/${plugin}.so"
            exit 1
        fi
    done
    
    print_info "Test environment ready"
}

# Test 1: Command-Line Argument Testing
test_argument_parsing() {
    print_test_header "Command-Line Argument Testing"
    
    # Test 1.1: No arguments
    print_test_case "No arguments"
    count_test
    if run_with_timeout "${ANALYZER}" >/dev/null 2>&1; then
        print_fail "Should fail with no arguments"
    else
        print_pass "Correctly failed with no arguments"
    fi
    
    # Test 1.2: Missing plugins
    print_test_case "Missing plugins argument"
    count_test
    if run_with_timeout "${ANALYZER}" 10 >/dev/null 2>&1; then
        print_fail "Should fail with missing plugins"
    else
        print_pass "Correctly failed with missing plugins"
    fi
    
    # Test 1.3: Invalid queue size - zero
    print_test_case "Invalid queue size (zero)"
    count_test
    if run_with_timeout "${ANALYZER}" 0 logger >/dev/null 2>&1; then
        print_fail "Should fail with zero queue size"
    else
        print_pass "Correctly failed with zero queue size"
    fi
    
    # Test 1.4: Invalid queue size - negative
    print_test_case "Invalid queue size (negative)"
    count_test
    if run_with_timeout "${ANALYZER}" -5 logger >/dev/null 2>&1; then
        print_fail "Should fail with negative queue size"
    else
        print_pass "Correctly failed with negative queue size"
    fi
    
    # Test 1.5: Invalid queue size - non-numeric
    print_test_case "Invalid queue size (non-numeric)"
    count_test
    if run_with_timeout "${ANALYZER}" abc logger >/dev/null 2>&1; then
        print_fail "Should fail with non-numeric queue size"
    else
        print_pass "Correctly failed with non-numeric queue size"
    fi
    
    # Test 1.6: Non-existent plugin
    print_test_case "Non-existent plugin"
    count_test
    if run_with_timeout "${ANALYZER}" 10 nonexistent >/dev/null 2>&1; then
        print_fail "Should fail with non-existent plugin"
    else
        print_pass "Correctly failed with non-existent plugin"
    fi
    
    # Test 1.7: Valid arguments
    print_test_case "Valid arguments"
    count_test
    if echo "<END>" | run_with_timeout "${ANALYZER}" 10 logger >/dev/null 2>&1; then
        print_pass "Correctly accepted valid arguments"
    else
        print_fail "Should accept valid arguments"
    fi
}

# Test 2: Basic Plugin Functionality
test_basic_functionality() {
    print_test_header "Basic Plugin Functionality"
    
    # Test 2.1: Logger plugin
    print_test_case "Logger plugin"
    count_test
    EXPECTED="[logger] hello"
    ACTUAL=$(echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 10 logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Logger plugin works correctly"
    else
        print_fail "Logger plugin failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 2.2: Uppercaser plugin
    print_test_case "Uppercaser plugin"
    count_test
    EXPECTED="[logger] HELLO"
    ACTUAL=$(echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Uppercaser plugin works correctly"
    else
        print_fail "Uppercaser plugin failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 2.3: Rotator plugin
    print_test_case "Rotator plugin"
    count_test
    EXPECTED="[logger] ohell"
    ACTUAL=$(echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 10 rotator logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Rotator plugin works correctly"
    else
        print_fail "Rotator plugin failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 2.4: Flipper plugin
    print_test_case "Flipper plugin"
    count_test
    EXPECTED="[logger] olleh"
    ACTUAL=$(echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 10 flipper logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Flipper plugin works correctly"
    else
        print_fail "Flipper plugin failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 2.5: Expander plugin
    print_test_case "Expander plugin"
    count_test
    EXPECTED="[logger] h i"
    ACTUAL=$(echo -e "hi\n<END>" | run_with_timeout "${ANALYZER}" 10 expander logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Expander plugin works correctly"
    else
        print_fail "Expander plugin failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
}

# Test 3: Complex Pipeline Testing
test_complex_pipelines() {
    print_test_header "Complex Pipeline Testing"
    
    # Test 3.1: Multi-stage pipeline
    print_test_case "Multi-stage pipeline (uppercaser -> rotator -> logger)"
    count_test
    EXPECTED="[logger] EHLLO"
    ACTUAL=$(echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 20 uppercaser rotator logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Multi-stage pipeline works correctly"
    else
        print_fail "Multi-stage pipeline failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 3.2: Repeated plugins
    print_test_case "Repeated plugins (rotator -> rotator -> rotator)"
    count_test
    EXPECTED="[logger] cab"
    ACTUAL=$(echo -e "abc\n<END>" | run_with_timeout "${ANALYZER}" 5 rotator rotator rotator logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Repeated plugins work correctly"
    else
        print_fail "Repeated plugins failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 3.3: All plugins together
    print_test_case "All plugins pipeline"
    count_test
    # This is complex - just test that it doesn't crash
    if echo -e "test\n<END>" | run_with_timeout "${ANALYZER}" 20 uppercaser rotator expander flipper logger >/dev/null 2>&1; then
        print_pass "All plugins pipeline completed successfully"
    else
        print_fail "All plugins pipeline failed"
    fi
    
    # Test 3.4: Long pipeline with same plugin
    print_test_case "Long pipeline with repeated plugin"
    count_test
    if echo -e "hello\n<END>" | run_with_timeout "${ANALYZER}" 10 rotator rotator rotator rotator rotator logger >/dev/null 2>&1; then
        print_pass "Long pipeline works correctly"
    else
        print_fail "Long pipeline failed"
    fi
}

# Test 4: Edge Cases
test_edge_cases() {
    print_test_header "Edge Cases"
    
    # Test 4.1: Empty string
    print_test_case "Empty string input"
    count_test
    EXPECTED="[logger] "
    ACTUAL=$(echo -e "\n<END>" | run_with_timeout "${ANALYZER}" 10 logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Empty string handled correctly"
    else
        print_fail "Empty string handling failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 4.2: Single character
    print_test_case "Single character"
    count_test
    EXPECTED="[logger] A"
    ACTUAL=$(echo -e "a\n<END>" | run_with_timeout "${ANALYZER}" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Single character handled correctly"
    else
        print_fail "Single character handling failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 4.3: Special characters
    print_test_case "Special characters"
    count_test
    EXPECTED="[logger] !@#$%"
    ACTUAL=$(echo -e "!@#$%\n<END>" | run_with_timeout "${ANALYZER}" 10 logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Special characters handled correctly"
    else
        print_fail "Special characters handling failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 4.4: Numbers
    print_test_case "Numeric input"
    count_test
    EXPECTED="[logger] 12345"
    ACTUAL=$(echo -e "12345\n<END>" | run_with_timeout "${ANALYZER}" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Numeric input handled correctly"
    else
        print_fail "Numeric input handling failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
    
    # Test 4.5: Mixed case with numbers and symbols
    print_test_case "Mixed complex input"
    count_test
    EXPECTED="[logger] HELLO123!@#"
    ACTUAL=$(echo -e "Hello123!@#\n<END>" | run_with_timeout "${ANALYZER}" 10 uppercaser logger 2>/dev/null | grep "\[logger\]" || true)
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        print_pass "Mixed complex input handled correctly"
    else
        print_fail "Mixed complex input handling failed (Expected: '$EXPECTED', Got: '$ACTUAL')"
    fi
}

# Test 5: Graceful Shutdown Testing
test_graceful_shutdown() {
    print_test_header "Graceful Shutdown Testing"
    
    # Test 5.1: Immediate <END>
    print_test_case "Immediate <END> signal"
    count_test
    OUTPUT=$(echo "<END>" | run_with_timeout "${ANALYZER}" 10 logger 2>&1)
    if echo "$OUTPUT" | grep -q "Pipeline shutdown complete"; then
        print_pass "Immediate <END> handled correctly"
    else
        print_fail "Immediate <END> handling failed"
    fi
    
    # Test 5.2: Multiple lines then <END>
    print_test_case "Multiple lines then <END>"
    count_test
    OUTPUT=$(echo -e "line1\nline2\nline3\n<END>" | run_with_timeout "${ANALYZER}" 10 logger 2>&1)
    if echo "$OUTPUT" | grep -q "Pipeline shutdown complete"; then
        print_pass "Multiple lines with <END> handled correctly"
    else
        print_fail "Multiple lines with <END> handling failed"
    fi
    
    # Test 5.3: <END> with typewriter (test threading)
    print_test_case "<END> with slow typewriter plugin"
    count_test
    # This tests if the shutdown works even with the slow typewriter
    if echo -e "test\n<END>" | run_with_timeout "${ANALYZER}" 5 typewriter >/dev/null 2>&1; then
        print_pass "Shutdown with typewriter works correctly"
    else
        print_fail "Shutdown with typewriter failed"
    fi
}

# Test 6: Queue Capacity Testing
test_queue_capacity() {
    print_test_header "Queue Capacity Testing"
    
    # Test 6.1: Minimum queue size
    print_test_case "Minimum queue size (1)"
    count_test
    if echo -e "test\n<END>" | run_with_timeout "${ANALYZER}" 1 logger >/dev/null 2>&1; then
        print_pass "Minimum queue size works correctly"
    else
        print_fail "Minimum queue size failed"
    fi
    
    # Test 6.2: Large queue size
    print_test_case "Large queue size (1000)"
    count_test
    if echo -e "test\n<END>" | run_with_timeout "${ANALYZER}" 1000 logger >/dev/null 2>&1; then
        print_pass "Large queue size works correctly"
    else
        print_fail "Large queue size failed"
    fi
    
    # Test 6.3: Queue blocking with slow consumer
    print_test_case "Queue blocking behavior"
    count_test
    # Send multiple items to a slow typewriter with small queue
    if echo -e "item1\nitem2\nitem3\n<END>" | run_with_timeout "${ANALYZER}" 2 typewriter >/dev/null 2>&1; then
        print_pass "Queue blocking behavior works correctly"
    else
        print_fail "Queue blocking behavior failed"
    fi
}

# Test 7: Stress Testing
test_stress_scenarios() {
    print_test_header "Stress Testing"
    
    # Test 7.1: Many input lines
    print_test_case "Many input lines (50 lines)"
    count_test
    {
        for i in $(seq 1 50); do
            echo "line$i"
        done
        echo "<END>"
    } | run_with_timeout "${ANALYZER}" 20 logger >/dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        print_pass "Many input lines handled correctly"
    else
        print_fail "Many input lines test failed"
    fi
    
    # Test 7.2: Long string (close to 1024 limit)
    print_test_case "Long string input"
    count_test
    LONG_STRING=$(printf 'a%.0s' {1..500})  # 500 character string
    if echo -e "${LONG_STRING}\n<END>" | run_with_timeout "${ANALYZER}" 20 logger >/dev/null 2>&1; then
        print_pass "Long string handled correctly"
    else
        print_fail "Long string test failed"
    fi
    
    # Test 7.3: Rapid pipeline with many plugins
    print_test_case "Rapid processing with many plugins"
    count_test
    if echo -e "rapid\ntest\nprocessing\n<END>" | run_with_timeout "${ANALYZER}" 50 uppercaser rotator flipper expander logger >/dev/null 2>&1; then
        print_pass "Rapid processing works correctly"
    else
        print_fail "Rapid processing test failed"
    fi
}

# Test 8: Run Unit Tests (if available)
test_unit_tests() {
    print_test_header "Unit Tests"
    
    # Test monitor unit tests
    if [ -x "${OUTPUT_DIR}/test_monitor_comprehensive" ]; then
        print_test_case "Monitor comprehensive tests"
        count_test
        if run_with_timeout "${OUTPUT_DIR}/test_monitor_comprehensive" >/dev/null 2>&1; then
            print_pass "Monitor tests passed"
        else
            print_fail "Monitor tests failed"
        fi
    fi
    
    # Test consumer-producer unit tests
    if [ -x "${OUTPUT_DIR}/test_consumer_producer" ]; then
        print_test_case "Consumer-Producer tests"
        count_test
        if run_with_timeout "${OUTPUT_DIR}/test_consumer_producer" >/dev/null 2>&1; then
            print_pass "Consumer-Producer tests passed"
        else
            print_fail "Consumer-Producer tests failed"
        fi
    fi
    
    # Test plugin infrastructure
    if [ -x "${OUTPUT_DIR}/test_plugin_infra" ]; then
        print_test_case "Plugin Infrastructure tests"
        count_test
        if run_with_timeout "${OUTPUT_DIR}/test_plugin_infra" >/dev/null 2>&1; then
            print_pass "Plugin Infrastructure tests passed"
        else
            print_fail "Plugin Infrastructure tests failed"
        fi
    fi
    
    # Test plugin direct tests
    if [ -x "${OUTPUT_DIR}/test_plugin_direct" ]; then
        print_test_case "Plugin Direct tests"
        count_test
        if run_with_timeout "${OUTPUT_DIR}/test_plugin_direct" >/dev/null 2>&1; then
            print_pass "Plugin Direct tests passed"
        else
            print_fail "Plugin Direct tests failed"
        fi
    fi
}

# Test 9: Memory Testing (if valgrind available)
test_memory_management() {
    print_test_header "Memory Management Testing"
    
    if command -v valgrind >/dev/null 2>&1; then
        print_test_case "Memory leak detection with Valgrind"
        count_test
        
        # Run a simple test with valgrind
        echo -e "hello\nworld\n<END>" > "${TEST_OUTPUT_DIR}/valgrind_input.txt"
        
        if valgrind --leak-check=full --error-exitcode=1 --quiet \
            "${ANALYZER}" 10 uppercaser logger \
            < "${TEST_OUTPUT_DIR}/valgrind_input.txt" \
            > "${TEST_OUTPUT_DIR}/valgrind_output.txt" 2>&1; then
            print_pass "No memory leaks detected"
        else
            print_fail "Memory leaks detected - check ${TEST_OUTPUT_DIR}/valgrind_output.txt"
        fi
        
        rm -f "${TEST_OUTPUT_DIR}/valgrind_input.txt"
    else
        print_warning "Valgrind not available - skipping memory leak tests"
    fi
}

# Generate final report
generate_report() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${BLUE}FINAL TEST REPORT${NC}"
    echo -e "${CYAN}========================================${NC}"
    
    echo -e "${GREEN}Tests Passed: ${TESTS_PASSED}${NC}"
    echo -e "${RED}Tests Failed: ${TESTS_FAILED}${NC}"
    echo -e "Total Tests:  ${TESTS_TOTAL}"
    
    if [ ${TESTS_TOTAL} -gt 0 ]; then
        PASS_RATE=$(echo "scale=1; ${TESTS_PASSED} * 100 / ${TESTS_TOTAL}" | bc -l 2>/dev/null || echo "N/A")
        echo -e "Pass Rate:    ${PASS_RATE}%"
    fi
    
    # Create summary report
    cat > "${TEST_OUTPUT_DIR}/test_report.txt" << EOF
Test Execution Report
====================
Date: $(date)
Total Tests: ${TESTS_TOTAL}
Passed: ${TESTS_PASSED}
Failed: ${TESTS_FAILED}
Pass Rate: ${PASS_RATE}%

Status: $([ ${TESTS_FAILED} -eq 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED")
EOF
    
    print_info "Detailed report saved to: ${TEST_OUTPUT_DIR}/test_report.txt"
    
    if [ ${TESTS_FAILED} -gt 0 ]; then
        echo -e "\n${RED}RESULT: FAILURE - ${TESTS_FAILED} tests failed!${NC}"
        echo -e "${YELLOW}Check the test output above for details.${NC}"
        return 1
    else
        echo -e "\n${GREEN}RESULT: SUCCESS - All tests passed!${NC}"
        echo -e "${GREEN}System is working correctly.${NC}"
        return 0
    fi
}

# Main test execution
main() {
    echo -e "${CYAN}========================================"
    echo -e "  MODULAR PIPELINE SYSTEM"
    echo -e "     COMPREHENSIVE TEST SUITE"
    echo -e "========================================"
    echo -e "${BLUE}Starting comprehensive testing...${NC}"
    
    # Setup
    setup_tests
    
    # Run all test categories
    test_argument_parsing
    test_basic_functionality
    test_complex_pipelines
    test_edge_cases
    test_graceful_shutdown
    test_queue_capacity
    test_stress_scenarios
    test_unit_tests
    test_memory_management
    
    # Generate final report
    if generate_report; then
        exit 0
    else
        exit 1
    fi
}

# Check if bc is available for calculations, install if needed
if ! command -v bc >/dev/null 2>&1; then
    print_warning "bc calculator not found - percentage calculations may not work"
fi

# Run main function
main "$@"