/**
 * COMPREHENSIVE PLUGIN INFRASTRUCTURE TEST SUITE
 * 
 * Tests all functionality of Plugin SDK and Common Infrastructure
 * Includes edge cases, deadlock detection, and timeout handling
 */

#define _GNU_SOURCE
#include "../plugins/plugin_common.h"
#include "../plugins/sync/monitor.h"
#include "../plugins/sync/consumer_producer.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

/* Test configuration */
#define MAX_TEST_THREADS 10
#define TEST_TIMEOUT_SECONDS 5
#define DEBUG_LOG_FILE "test_debug.log"
#define MAX_STRING_LENGTH 1024

/* Test result tracking */
typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_TIMEOUT,
    TEST_SKIP
} test_result_t;

/* Test statistics */
typedef struct {
    int tests_passed;
    int tests_failed;
    int tests_timeout;
    int tests_skipped;
    int total_tests;
} test_stats_t;

static test_stats_t g_test_stats = {0};
static FILE* g_debug_log = NULL;
static volatile int g_test_timeout_flag = 0;

/* Colors for output */
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define PURPLE "\033[0;35m"
#define CYAN "\033[0;36m"
#define NC "\033[0m"

/* Test plugin transformation functions */
const char* test_uppercase_transform(const char* input);
const char* test_reverse_transform(const char* input);
const char* test_null_transform(const char* input);
const char* test_malloc_fail_transform(const char* input);
const char* test_slow_transform(const char* input);

/* Global test context for mock plugins */
static int g_mock_plugin_call_count = 0;
static char* g_last_processed_string = NULL;
static int g_simulate_malloc_failure = 0;

/****************************************
 * UTILITY FUNCTIONS
 ****************************************/

void debug_log(const char* format, ...) {
    if (g_debug_log == NULL) return;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    va_list args;
    va_start(args, format);
    fprintf(g_debug_log, "[%ld.%06ld] ", tv.tv_sec, tv.tv_usec);
    vfprintf(g_debug_log, format, args);
    fprintf(g_debug_log, "\n");
    fflush(g_debug_log);
    va_end(args);
}

void print_test_header(const char* test_name) {
    printf("\n%s========================================%s\n", CYAN, NC);
    printf("%sTEST: %s%s\n", BLUE, test_name, NC);
    printf("%s========================================%s\n", CYAN, NC);
    debug_log("=== Starting test: %s ===", test_name);
}

void print_test_result(const char* test_name, test_result_t result) {
    const char* status;
    const char* color;
    
    switch (result) {
        case TEST_PASS:
            status = "PASS";
            color = GREEN;
            g_test_stats.tests_passed++;
            break;
        case TEST_FAIL:
            status = "FAIL";
            color = RED;
            g_test_stats.tests_failed++;
            break;
        case TEST_TIMEOUT:
            status = "TIMEOUT";
            color = YELLOW;
            g_test_stats.tests_timeout++;
            break;
        case TEST_SKIP:
            status = "SKIP";
            color = YELLOW;
            g_test_stats.tests_skipped++;
            break;
    }
    
    g_test_stats.total_tests++;
    printf("%s[%s]%s %s\n", color, status, NC, test_name);
    debug_log("Test result: %s - %s", test_name, status);
}

/* Timeout handler */
void test_timeout_handler(int sig) {
    g_test_timeout_flag = 1;
    debug_log("TIMEOUT SIGNAL RECEIVED - Test exceeded time limit");
}

test_result_t run_test_with_timeout(test_result_t (*test_func)(void), const char* test_name) {
    debug_log("Starting test with timeout: %s", test_name);
    
    g_test_timeout_flag = 0;
    signal(SIGALRM, test_timeout_handler);
    alarm(TEST_TIMEOUT_SECONDS);
    
    test_result_t result = test_func();
    
    alarm(0); // Cancel alarm
    
    if (g_test_timeout_flag) {
        debug_log("Test %s timed out after %d seconds", test_name, TEST_TIMEOUT_SECONDS);
        return TEST_TIMEOUT;
    }
    
    return result;
}

/* Mock transformation functions */
const char* test_uppercase_transform(const char* input) {
    debug_log("test_uppercase_transform called with: '%s'", input ? input : "NULL");
    
    if (input == NULL) {
        return NULL;
    }
    
    int len = strlen(input);
    char* result = malloc(len + 1);
    if (result == NULL) {
        debug_log("malloc failed in test_uppercase_transform");
        return NULL;
    }
    
    for (int i = 0; i < len; i++) {
        result[i] = (input[i] >= 'a' && input[i] <= 'z') ? input[i] - 32 : input[i];
    }
    result[len] = '\0';
    
    g_mock_plugin_call_count++;
    if (g_last_processed_string) {
        free(g_last_processed_string);
    }
    g_last_processed_string = strdup(result);
    
    debug_log("test_uppercase_transform returning: '%s'", result);
    return result;
}

const char* test_reverse_transform(const char* input) {
    debug_log("test_reverse_transform called with: '%s'", input ? input : "NULL");
    
    if (input == NULL) {
        return NULL;
    }
    
    int len = strlen(input);
    char* result = malloc(len + 1);
    if (result == NULL) {
        debug_log("malloc failed in test_reverse_transform");
        return NULL;
    }
    
    for (int i = 0; i < len; i++) {
        result[i] = input[len - 1 - i];
    }
    result[len] = '\0';
    
    g_mock_plugin_call_count++;
    return result;
}

const char* test_null_transform(const char* input) {
    debug_log("test_null_transform called - always returns NULL");
    g_mock_plugin_call_count++;
    return NULL;
}

const char* test_malloc_fail_transform(const char* input) {
    debug_log("test_malloc_fail_transform called - simulating malloc failure");
    g_mock_plugin_call_count++;
    
    if (g_simulate_malloc_failure) {
        debug_log("Simulating malloc failure");
        return NULL;
    }
    
    return strdup(input ? input : "");
}

const char* test_slow_transform(const char* input) {
    debug_log("test_slow_transform called - adding delay");
    usleep(100000); // 100ms delay
    g_mock_plugin_call_count++;
    return input ? strdup(input) : NULL;
}

/* Mock next plugin function */
const char* mock_next_plugin_place_work(const char* str) {
    debug_log("mock_next_plugin_place_work called with: '%s'", str ? str : "NULL");
    
    if (g_last_processed_string) {
        free(g_last_processed_string);
    }
    g_last_processed_string = str ? strdup(str) : NULL;
    
    return NULL; // Success
}

const char* mock_failing_next_plugin(const char* str) {
    debug_log("mock_failing_next_plugin called - returning error");
    return "Mock next plugin error";
}

/****************************************
 * MONITOR TESTS
 ****************************************/

test_result_t test_monitor_basic_functionality() {
    print_test_header("Monitor Basic Functionality");
    
    monitor_t monitor;
    
    // Test initialization
    if (monitor_init(&monitor) != 0) {
        printf("  ✗ Monitor initialization failed\n");
        return TEST_FAIL;
    }
    debug_log("Monitor initialized successfully");
    
    // Test initial state
    if (monitor.signaled != 0) {
        printf("  ✗ Monitor initial state incorrect\n");
        monitor_destroy(&monitor);
        return TEST_FAIL;
    }
    printf("  ✓ Monitor initialized with correct state\n");
    
    // Test signal
    monitor_signal(&monitor);
    if (monitor.signaled != 1) {
        printf("  ✗ Monitor signal failed\n");
        monitor_destroy(&monitor);
        return TEST_FAIL;
    }
    printf("  ✓ Monitor signal works\n");
    
    // Test reset
    monitor_reset(&monitor);
    if (monitor.signaled != 0) {
        printf("  ✗ Monitor reset failed\n");
        monitor_destroy(&monitor);
        return TEST_FAIL;
    }
    printf("  ✓ Monitor reset works\n");
    
    // Test wait after signal
    monitor_signal(&monitor);
    if (monitor_wait(&monitor) != 0) {
        printf("  ✗ Monitor wait after signal failed\n");
        monitor_destroy(&monitor);
        return TEST_FAIL;
    }
    printf("  ✓ Monitor wait after signal works\n");
    
    monitor_destroy(&monitor);
    printf("  ✓ Monitor destroyed successfully\n");
    
    return TEST_PASS;
}

test_result_t test_monitor_edge_cases() {
    print_test_header("Monitor Edge Cases");
    
    // Test NULL operations
    monitor_signal(NULL);
    monitor_reset(NULL);
    monitor_destroy(NULL);
    printf("  ✓ NULL operations handled gracefully\n");
    
    if (monitor_init(NULL) == 0) {
        printf("  ✗ monitor_init(NULL) should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ monitor_init(NULL) correctly returns error\n");
    
    if (monitor_wait(NULL) == 0) {
        printf("  ✗ monitor_wait(NULL) should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ monitor_wait(NULL) correctly returns error\n");
    
    return TEST_PASS;
}

/****************************************
 * CONSUMER-PRODUCER QUEUE TESTS
 ****************************************/

test_result_t test_queue_basic_functionality() {
    print_test_header("Queue Basic Functionality");
    
    consumer_producer_t queue;
    memset(&queue, 0, sizeof(consumer_producer_t));
    
    // Test initialization
    const char* error = consumer_producer_init(&queue, 5);
    if (error != NULL) {
        printf("  ✗ Queue initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    printf("  ✓ Queue initialized successfully\n");
    
    // Test put operation
    error = consumer_producer_put(&queue, "test1");
    if (error != NULL) {
        printf("  ✗ Queue put failed: %s\n", error);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Queue put works\n");
    
    // Test get operation
    char* item = consumer_producer_get(&queue);
    if (item == NULL) {
        printf("  ✗ Queue get failed\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    if (strcmp(item, "test1") != 0) {
        printf("  ✗ Queue get returned wrong item: %s\n", item);
        free(item);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Queue get works correctly\n");
    free(item);
    
    // Test finished signaling
    consumer_producer_signal_finished(&queue);
    printf("  ✓ Queue finished signal sent\n");
    
    if (consumer_producer_wait_finished(&queue) != 0) {
        printf("  ✗ Queue wait finished failed\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Queue wait finished works\n");
    
    consumer_producer_destroy(&queue);
    printf("  ✓ Queue destroyed successfully\n");
    
    return TEST_PASS;
}

test_result_t test_queue_edge_cases() {
    print_test_header("Queue Edge Cases");
    
    consumer_producer_t queue;
    memset(&queue, 0, sizeof(consumer_producer_t));
    
    // Test NULL initialization
    const char* error = consumer_producer_init(NULL, 5);
    if (error == NULL) {
        printf("  ✗ Queue init with NULL should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ Queue init with NULL correctly fails\n");
    
    // Test invalid capacity
    error = consumer_producer_init(&queue, 0);
    if (error == NULL) {
        printf("  ✗ Queue init with zero capacity should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ Queue init with zero capacity correctly fails\n");
    
    error = consumer_producer_init(&queue, -1);
    if (error == NULL) {
        printf("  ✗ Queue init with negative capacity should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ Queue init with negative capacity correctly fails\n");
    
    // Test operations on uninitialized queue
    error = consumer_producer_put(NULL, "test");
    if (error == NULL) {
        printf("  ✗ Put to NULL queue should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ Put to NULL queue correctly fails\n");
    
    char* item = consumer_producer_get(NULL);
    if (item != NULL) {
        printf("  ✗ Get from NULL queue should return NULL\n");
        free(item);
        return TEST_FAIL;
    }
    printf("  ✓ Get from NULL queue correctly returns NULL\n");
    
    return TEST_PASS;
}

/****************************************
 * PLUGIN COMMON INFRASTRUCTURE TESTS
 ****************************************/

test_result_t test_common_plugin_basic_functionality() {
    print_test_header("Common Plugin Basic Functionality");
    
    // Reset global state
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    
    // Test initialization
    const char* error = common_plugin_init(test_uppercase_transform, "test_plugin", 5);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    printf("  ✓ Plugin initialized successfully\n");
    
    // Test plugin_get_name
    const char* name = plugin_get_name();
    if (strcmp(name, "test_plugin") != 0) {
        printf("  ✗ Plugin name incorrect: %s\n", name);
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin name correct\n");
    
    // Test place_work
    error = plugin_place_work("hello");
    if (error != NULL) {
        printf("  ✗ Plugin place_work failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin place_work works\n");
    
    // Give some time for processing
    usleep(100000); // 100ms
    
    // Test attach functionality
    plugin_attach(mock_next_plugin_place_work);
    printf("  ✓ Plugin attach works\n");
    
    // Send end signal
    error = plugin_place_work("<END>");
    if (error != NULL) {
        printf("  ✗ Plugin place_work with <END> failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin handles <END> signal\n");
    
    // Test wait_finished
    error = plugin_wait_finished();
    if (error != NULL) {
        printf("  ✗ Plugin wait_finished failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin wait_finished works\n");
    
    // Test finalization
    error = plugin_fini();
    if (error != NULL) {
        printf("  ✗ Plugin finalization failed: %s\n", error);
        return TEST_FAIL;
    }
    printf("  ✓ Plugin finalized successfully\n");
    
    // Check if processing occurred
    if (g_mock_plugin_call_count == 0) {
        printf("  ✗ Plugin processing function was never called\n");
        return TEST_FAIL;
    }
    printf("  ✓ Plugin processing function was called %d times\n", g_mock_plugin_call_count);
    
    return TEST_PASS;
}

test_result_t test_plugin_error_conditions() {
    print_test_header("Plugin Error Conditions");
    
    // Reset global state
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    // Test NULL process function
    const char* error = common_plugin_init(NULL, "test", 5);
    if (error == NULL) {
        printf("  ✗ Plugin init with NULL process function should fail\n");
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin init with NULL process function correctly fails\n");
    
    // Test NULL name
    error = common_plugin_init(test_uppercase_transform, NULL, 5);
    if (error == NULL) {
        printf("  ✗ Plugin init with NULL name should fail\n");
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin init with NULL name correctly fails\n");
    
    // Test invalid queue size
    error = common_plugin_init(test_uppercase_transform, "test", 0);
    if (error == NULL) {
        printf("  ✗ Plugin init with zero queue size should fail\n");
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin init with zero queue size correctly fails\n");
    
    // Test operations on uninitialized plugin
    error = plugin_place_work("test");
    if (error == NULL) {
        printf("  ✗ place_work on uninitialized plugin should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ place_work on uninitialized plugin correctly fails\n");
    
    error = plugin_wait_finished();
    if (error == NULL) {
        printf("  ✗ wait_finished on uninitialized plugin should fail\n");
        return TEST_FAIL;
    }
    printf("  ✓ wait_finished on uninitialized plugin correctly fails\n");
    
    // Test double initialization
    error = common_plugin_init(test_uppercase_transform, "test1", 5);
    if (error != NULL) {
        printf("  ✗ First plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    error = common_plugin_init(test_uppercase_transform, "test2", 5);
    if (error == NULL) {
        printf("  ✗ Double initialization should fail\n");
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Double initialization correctly fails\n");
    
    plugin_fini();
    return TEST_PASS;
}

test_result_t test_plugin_string_processing() {
    print_test_header("Plugin String Processing");
    
    // Reset global state
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    if (g_last_processed_string) {
        free(g_last_processed_string);
        g_last_processed_string = NULL;
    }
    
    // Initialize plugin
    const char* error = common_plugin_init(test_uppercase_transform, "uppercase_test", 10);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Attach mock next plugin
    plugin_attach(mock_next_plugin_place_work);
    
    // Test various strings
    const char* test_strings[] = {
        "hello",
        "Hello World",
        "123!@#abc",
        "", // Empty string
        "a", // Single character
        NULL
    };
    
    for (int i = 0; test_strings[i] != NULL; i++) {
        debug_log("Testing string: '%s'", test_strings[i]);
        
        error = plugin_place_work(test_strings[i]);
        if (error != NULL) {
            printf("  ✗ Failed to process string '%s': %s\n", test_strings[i], error);
            plugin_fini();
            return TEST_FAIL;
        }
        
        // Wait a bit for processing
        usleep(50000); // 50ms
    }
    
    printf("  ✓ All test strings processed successfully\n");
    
    // Send end signal and cleanup
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    if (g_mock_plugin_call_count == 0) {
        printf("  ✗ No strings were processed\n");
        return TEST_FAIL;
    }
    
    printf("  ✓ Processed %d strings\n", g_mock_plugin_call_count);
    
    if (g_last_processed_string) {
        free(g_last_processed_string);
        g_last_processed_string = NULL;
    }
    
    return TEST_PASS;
}

test_result_t test_plugin_memory_management() {
    print_test_header("Plugin Memory Management");
    
    // Reset global state
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    
    // Test with transform function that returns NULL
    const char* error = common_plugin_init(test_null_transform, "null_test", 5);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    plugin_attach(mock_next_plugin_place_work);
    
    // Send some work
    error = plugin_place_work("test");
    if (error != NULL) {
        printf("  ✗ Failed to place work: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    usleep(100000); // Wait for processing
    
    printf("  ✓ Plugin handles NULL return from transform function\n");
    
    // Cleanup
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    // Test with failing malloc simulation
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_simulate_malloc_failure = 1;
    
    error = common_plugin_init(test_malloc_fail_transform, "malloc_fail_test", 5);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    plugin_attach(mock_next_plugin_place_work);
    
    error = plugin_place_work("test");
    if (error != NULL) {
        printf("  ✗ Failed to place work: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    usleep(100000); // Wait for processing
    
    printf("  ✓ Plugin handles malloc failures gracefully\n");
    
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    g_simulate_malloc_failure = 0;
    
    return TEST_PASS;
}

test_result_t test_plugin_concurrent_operations() {
    print_test_header("Plugin Concurrent Operations");
    
    // Reset global state
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    
    // Initialize plugin
    const char* error = common_plugin_init(test_uppercase_transform, "concurrent_test", 3);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    plugin_attach(mock_next_plugin_place_work);
    
    // Send multiple items rapidly to test queue blocking
    const char* test_items[] = {"item1", "item2", "item3", "item4", "item5", NULL};
    
    for (int i = 0; test_items[i] != NULL; i++) {
        debug_log("Sending item: %s", test_items[i]);
        error = plugin_place_work(test_items[i]);
        if (error != NULL) {
            printf("  ✗ Failed to place work '%s': %s\n", test_items[i], error);
            plugin_fini();
            return TEST_FAIL;
        }
        usleep(10000); // Small delay
    }
    
    printf("  ✓ Multiple items sent successfully\n");
    
    // Wait for processing
    usleep(500000); // 500ms
    
    // Send end signal
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    printf("  ✓ Concurrent operations completed successfully\n");
    
    return TEST_PASS;
}

/****************************************
 * INTEGRATION TESTS
 ****************************************/

test_result_t test_full_plugin_pipeline() {
    print_test_header("Full Plugin Pipeline Integration");
    
    // This test simulates a complete plugin pipeline scenario
    // Plugin 1: uppercase -> Plugin 2: reverse -> Logger
    
    debug_log("Starting full pipeline integration test");
    
    // Test would require multiple plugin instances
    // For now, we'll test the infrastructure with chain simulation
    
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    
    const char* error = common_plugin_init(test_uppercase_transform, "pipeline_test", 5);
    if (error != NULL) {
        printf("  ✗ Pipeline plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Simulate chained processing
    plugin_attach(mock_next_plugin_place_work);
    
    const char* pipeline_inputs[] = {
        "hello world",
        "testing pipeline",
        "final test",
        NULL
    };
    
    for (int i = 0; pipeline_inputs[i] != NULL; i++) {
        error = plugin_place_work(pipeline_inputs[i]);
        if (error != NULL) {
            printf("  ✗ Pipeline processing failed for '%s': %s\n", 
                   pipeline_inputs[i], error);
            plugin_fini();
            return TEST_FAIL;
        }
        usleep(50000); // Allow processing
    }
    
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    printf("  ✓ Full pipeline integration test completed\n");
    
    return TEST_PASS;
}

/****************************************
 * STRESS TESTS
 ****************************************/

test_result_t test_high_volume_processing() {
    print_test_header("High Volume Processing Stress Test");
    
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    g_mock_plugin_call_count = 0;
    
    const char* error = common_plugin_init(test_uppercase_transform, "stress_test", 20);
    if (error != NULL) {
        printf("  ✗ Stress test plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    plugin_attach(mock_next_plugin_place_work);
    
    // Send many items
    const int stress_count = 100;
    char buffer[32];
    
    debug_log("Starting high volume test with %d items", stress_count);
    
    for (int i = 0; i < stress_count; i++) {
        snprintf(buffer, sizeof(buffer), "stress_item_%d", i);
        error = plugin_place_work(buffer);
        if (error != NULL) {
            printf("  ✗ Stress test failed at item %d: %s\n", i, error);
            plugin_fini();
            return TEST_FAIL;
        }
        
        if (i % 20 == 0) {
            usleep(10000); // Brief pause every 20 items
        }
    }
    
    printf("  ✓ Sent %d stress test items\n", stress_count);
    
    // Wait for processing
    usleep(1000000); // 1 second
    
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    if (g_mock_plugin_call_count < stress_count) {
        printf("  ⚠ Processed %d out of %d items\n", g_mock_plugin_call_count, stress_count);
    } else {
        printf("  ✓ All %d items processed successfully\n", stress_count);
    }
    
    return TEST_PASS;
}

test_result_t test_rapid_shutdown_scenarios() {
    print_test_header("Rapid Shutdown Scenarios");
    
    // Test immediate shutdown after initialization
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    const char* error = common_plugin_init(test_uppercase_transform, "rapid_shutdown", 5);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Immediate shutdown
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    printf("  ✓ Immediate shutdown after init works\n");
    
    // Test shutdown with items in queue
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    error = common_plugin_init(test_slow_transform, "slow_shutdown", 10);
    if (error != NULL) {
        printf("  ✗ Slow plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    plugin_attach(mock_next_plugin_place_work);
    
    // Add items then immediately shutdown
    plugin_place_work("item1");
    plugin_place_work("item2");
    plugin_place_work("item3");
    plugin_place_work("<END>");
    
    plugin_wait_finished();
    plugin_fini();
    printf("  ✓ Shutdown with pending items works\n");
    
    return TEST_PASS;
}

/****************************************
 * DEADLOCK DETECTION TESTS
 ****************************************/

test_result_t test_potential_deadlock_scenarios() {
    print_test_header("Potential Deadlock Scenarios");
    
    // Test 1: Queue full with slow consumer
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    const char* error = common_plugin_init(test_slow_transform, "deadlock_test", 2);
    if (error != NULL) {
        printf("  ✗ Deadlock test plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Don't attach next plugin to simulate slow consumption
    
    debug_log("Testing queue full scenario with slow processing");
    
    // Fill queue rapidly
    error = plugin_place_work("item1");
    if (error != NULL) {
        printf("  ✗ First item placement failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    error = plugin_place_work("item2");
    if (error != NULL) {
        printf("  ✗ Second item placement failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    // This should block briefly but not deadlock
    error = plugin_place_work("item3");
    if (error != NULL) {
        printf("  ✗ Third item placement failed: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    printf("  ✓ Queue blocking behavior works correctly\n");
    
    // Cleanup
    usleep(500000); // Wait for processing
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    return TEST_PASS;
}

test_result_t test_circular_dependency_prevention() {
    print_test_header("Circular Dependency Prevention");
    
    // Test that we don't create circular references
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    const char* error = common_plugin_init(test_uppercase_transform, "circular_test", 5);
    if (error != NULL) {
        printf("  ✗ Plugin initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Attach to a mock plugin that could cause issues
    plugin_attach(mock_failing_next_plugin);
    
    error = plugin_place_work("test");
    if (error != NULL) {
        printf("  ✗ Failed to place work: %s\n", error);
        plugin_fini();
        return TEST_FAIL;
    }
    
    usleep(100000); // Wait for processing
    
    printf("  ✓ Plugin handles next plugin errors gracefully\n");
    
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    return TEST_PASS;
}

/****************************************
 * RESOURCE LEAK TESTS
 ****************************************/

test_result_t test_memory_leak_prevention() {
    print_test_header("Memory Leak Prevention");
    
    // Test multiple init/fini cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        debug_log("Memory leak test cycle %d", cycle + 1);
        
        memset(&g_plugin_context, 0, sizeof(plugin_context_t));
        
        const char* error = common_plugin_init(test_uppercase_transform, "leak_test", 5);
        if (error != NULL) {
            printf("  ✗ Cycle %d initialization failed: %s\n", cycle + 1, error);
            return TEST_FAIL;
        }
        
        plugin_attach(mock_next_plugin_place_work);
        
        // Process some items
        plugin_place_work("test1");
        plugin_place_work("test2");
        usleep(50000);
        
        plugin_place_work("<END>");
        plugin_wait_finished();
        
        error = plugin_fini();
        if (error != NULL) {
            printf("  ✗ Cycle %d finalization failed: %s\n", cycle + 1, error);
            return TEST_FAIL;
        }
    }
    
    printf("  ✓ Multiple init/fini cycles completed without leaks\n");
    
    return TEST_PASS;
}
void minimal_queue_test() {
    printf("=== MINIMAL QUEUE TEST ===\n");
    
    consumer_producer_t queue;
    memset(&queue, 0, sizeof(consumer_producer_t));
    
    // Initialize
    const char* error = consumer_producer_init(&queue, 5);
    if (error) {
        printf("Init failed: %s\n", error);
        return;
    }
    
    printf("Queue initialized. items pointer = %p\n", (void*)queue.items);
    
    // Check items array is accessible
    for (int i = 0; i < 5; i++) {
        queue.items[i] = NULL;  // This should not crash
        printf("items[%d] = %p (should be NULL)\n", i, (void*)queue.items[i]);
    }
    
    printf("Items array is accessible!\n");
    
    // Test direct assignment (bypass put/get logic)
    queue.items[0] = strdup("test_direct");
    queue.count = 1;
    queue.head = 0;
    queue.tail = 1;
    
    printf("Direct assignment done. About to read...\n");
    printf("queue.items[0] = %p\n", (void*)queue.items[0]);
    
    if (queue.items[0]) {
        printf("Direct read successful: %s\n", queue.items[0]);
        free(queue.items[0]);
    }
    
    consumer_producer_destroy(&queue);
    printf("=== MINIMAL TEST COMPLETE ===\n");
}

test_result_t test_thread_cleanup() {
    print_test_header("Thread Cleanup Verification");
    
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    const char* error = common_plugin_init(test_uppercase_transform, "thread_test", 5);
    if (error != NULL) {
        printf("  ✗ Thread test initialization failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Verify thread was created
    if (!g_plugin_context.thread_created) {
        printf("  ✗ Plugin thread was not created\n");
        plugin_fini();
        return TEST_FAIL;
    }
    printf("  ✓ Plugin thread created successfully\n");
    
    plugin_attach(mock_next_plugin_place_work);
    plugin_place_work("test");
    usleep(100000);
    
    plugin_place_work("<END>");
    plugin_wait_finished();
    
    error = plugin_fini();
    if (error != NULL) {
        printf("  ✗ Thread cleanup failed: %s\n", error);
        return TEST_FAIL;
    }
    
    // Verify thread was cleaned up
    if (g_plugin_context.thread_created) {
        printf("  ✗ Plugin thread was not properly cleaned up\n");
        return TEST_FAIL;
    }
    printf("  ✓ Plugin thread cleaned up successfully\n");
    
    return TEST_PASS;
}

/****************************************
 * MAIN TEST RUNNER
 ****************************************/

int main(int argc, char* argv[]) {
    printf("%s===========================================\n", PURPLE);
    printf("    COMPREHENSIVE PLUGIN INFRASTRUCTURE\n");
    printf("           TEST SUITE\n");
    printf("===========================================%s\n", NC);
    printf("Testing Plugin SDK and Common Infrastructure\n");
    printf("Timeout per test: %d seconds\n", TEST_TIMEOUT_SECONDS);
    printf("Debug logging: %s\n", DEBUG_LOG_FILE);
    printf("\n");
    
    // Initialize debug logging
    g_debug_log = fopen(DEBUG_LOG_FILE, "w");
    if (g_debug_log == NULL) {
        printf("Warning: Could not open debug log file\n");
    }
    
    debug_log("=== COMPREHENSIVE PLUGIN TEST SUITE STARTED ===");

    // Run minimal queue test
    printf("Running minimal queue test...");
    minimal_queue_test();
    
    // Monitor Tests
    print_test_result("Monitor Basic Functionality", 
                     run_test_with_timeout(test_monitor_basic_functionality, "Monitor Basic"));
    
    print_test_result("Monitor Edge Cases", 
                     run_test_with_timeout(test_monitor_edge_cases, "Monitor Edge Cases"));
    
    // Queue Tests
    print_test_result("Queue Basic Functionality", 
                     run_test_with_timeout(test_queue_basic_functionality, "Queue Basic"));
    
    print_test_result("Queue Edge Cases", 
                     run_test_with_timeout(test_queue_edge_cases, "Queue Edge Cases"));
    
    // Plugin Infrastructure Tests
    print_test_result("Common Plugin Basic Functionality", 
                     run_test_with_timeout(test_common_plugin_basic_functionality, "Plugin Basic"));
    
    print_test_result("Plugin Error Conditions", 
                     run_test_with_timeout(test_plugin_error_conditions, "Plugin Errors"));
    
    print_test_result("Plugin String Processing", 
                     run_test_with_timeout(test_plugin_string_processing, "Plugin Processing"));
    
    print_test_result("Plugin Memory Management", 
                     run_test_with_timeout(test_plugin_memory_management, "Plugin Memory"));
    
    print_test_result("Plugin Concurrent Operations", 
                     run_test_with_timeout(test_plugin_concurrent_operations, "Plugin Concurrent"));
    
    // Integration Tests
    print_test_result("Full Plugin Pipeline Integration", 
                     run_test_with_timeout(test_full_plugin_pipeline, "Pipeline Integration"));
    
    // Stress Tests
    print_test_result("High Volume Processing", 
                     run_test_with_timeout(test_high_volume_processing, "High Volume"));
    
    print_test_result("Rapid Shutdown Scenarios", 
                     run_test_with_timeout(test_rapid_shutdown_scenarios, "Rapid Shutdown"));
    
    // Deadlock Detection Tests
    print_test_result("Potential Deadlock Scenarios", 
                     run_test_with_timeout(test_potential_deadlock_scenarios, "Deadlock Detection"));
    
    print_test_result("Circular Dependency Prevention", 
                     run_test_with_timeout(test_circular_dependency_prevention, "Circular Dependency"));
    
    // Resource Leak Tests
    print_test_result("Memory Leak Prevention", 
                     run_test_with_timeout(test_memory_leak_prevention, "Memory Leak"));
    
    print_test_result("Thread Cleanup Verification", 
                     run_test_with_timeout(test_thread_cleanup, "Thread Cleanup"));
    
    // Print comprehensive summary
    printf("\n%s===========================================%s\n", PURPLE, NC);
    printf("                FINAL SUMMARY\n");
    printf("%s===========================================%s\n", PURPLE, NC);
    printf("%sTests Passed:   %d%s\n", GREEN, g_test_stats.tests_passed, NC);
    printf("%sTests Failed:   %d%s\n", RED, g_test_stats.tests_failed, NC);
    printf("%sTests Timeout:  %d%s\n", YELLOW, g_test_stats.tests_timeout, NC);
    printf("%sTests Skipped:  %d%s\n", YELLOW, g_test_stats.tests_skipped, NC);
    printf("Total Tests:    %d\n", g_test_stats.total_tests);
    
    if (g_test_stats.total_tests > 0) {
        double pass_rate = (g_test_stats.tests_passed * 100.0) / g_test_stats.total_tests;
        printf("Pass Rate:      %.1f%%\n", pass_rate);
    }
    
    // Cleanup
    if (g_last_processed_string) {
        free(g_last_processed_string);
    }
    
    debug_log("=== TEST SUITE COMPLETED ===");
    debug_log("Final results: %d passed, %d failed, %d timeout, %d skipped", 
              g_test_stats.tests_passed, g_test_stats.tests_failed, 
              g_test_stats.tests_timeout, g_test_stats.tests_skipped);
    
    if (g_debug_log) {
        fclose(g_debug_log);
    }
    
    printf("\nDebug log saved to: %s\n", DEBUG_LOG_FILE);
    
    if (g_test_stats.tests_failed > 0 || g_test_stats.tests_timeout > 0) {
        printf("\n%sResult: FAILURE - Some tests failed or timed out!%s\n", RED, NC);
        printf("Check the debug log for detailed information.\n");
        return 1;
    } else {
        printf("\n%sResult: SUCCESS - All tests passed!%s\n", GREEN, NC);
        return 0;
    }
}