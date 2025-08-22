/**
 * DIRECT PLUGIN TESTING WITHOUT MAIN
 * 
 * This test compiles plugins directly (no dynamic loading)
 * to test their functionality and the plugin infrastructure
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
#include <assert.h>

/* Colors for output */
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define CYAN "\033[0;36m"
#define NC "\033[0m"

/* Test result tracking */
typedef struct {
    int passed;
    int failed;
    int total;
} test_stats_t;

static test_stats_t stats = {0, 0, 0};

/* Global to capture output from plugins */
static char* g_captured_output = NULL;
static pthread_mutex_t g_output_mutex = PTHREAD_MUTEX_INITIALIZER;

/****************************************
 * PLUGIN TRANSFORMATION FUNCTIONS
 * Direct implementations for testing
 ****************************************/

/* Logger transform */
const char* logger_transform(const char* input) {
    if (NULL == input) return NULL;
    
    // For testing, capture output instead of printing
    pthread_mutex_lock(&g_output_mutex);
    if (g_captured_output) free(g_captured_output);
    asprintf(&g_captured_output, "[logger] %s", input);
    pthread_mutex_unlock(&g_output_mutex);
    
    return strdup(input);
}

/* Uppercaser transform */
const char* uppercaser_transform(const char* input) {
    if (NULL == input) return NULL;
    
    int len = strlen(input);
    char* result = malloc(len + 1);
    if (NULL == result) return NULL;
    
    for (int i = 0; i < len; i++) {
        result[i] = (input[i] >= 'a' && input[i] <= 'z') ? input[i] - 32 : input[i];
    }
    result[len] = '\0';
    return result;
}

/* Rotator transform */
const char* rotator_transform(const char* input) {
    if (NULL == input) return NULL;
    
    int len = strlen(input);
    if (0 == len) return strdup("");
    
    char* result = malloc(len + 1);
    if (NULL == result) return NULL;
    
    result[0] = input[len - 1];
    for (int i = 0; i < len - 1; i++) {
        result[i + 1] = input[i];
    }
    result[len] = '\0';
    return result;
}

/* Flipper transform */
const char* flipper_transform(const char* input) {
    if (NULL == input) return NULL;
    
    int len = strlen(input);
    char* result = malloc(len + 1);
    if (NULL == result) return NULL;
    
    for (int i = 0; i < len; i++) {
        result[i] = input[len - 1 - i];
    }
    result[len] = '\0';
    return result;
}

/* Expander transform */
const char* expander_transform(const char* input) {
    if (NULL == input) return NULL;
    
    int len = strlen(input);
    if (0 == len) return strdup("");
    
    int new_len = (len == 1) ? 1 : len + (len - 1);
    char* result = malloc(new_len + 1);
    if (NULL == result) return NULL;
    
    int j = 0;
    for (int i = 0; i < len; i++) {
        result[j++] = input[i];
        if (i < len - 1) result[j++] = ' ';
    }
    result[new_len] = '\0';
    return result;
}

/* Typewriter transform (simplified for testing) */
const char* typewriter_transform(const char* input) {
    if (NULL == input) return NULL;
    
    // For testing, just capture what would be printed
    pthread_mutex_lock(&g_output_mutex);
    if (g_captured_output) free(g_captured_output);
    asprintf(&g_captured_output, "[typewriter] %s", input);
    pthread_mutex_unlock(&g_output_mutex);
    
    return strdup(input);
}

/****************************************
 * TEST HELPER FUNCTIONS
 ****************************************/

void print_test_header(const char* test_name) {
    printf("\n%s========================================%s\n", CYAN, NC);
    printf("%sTEST: %s%s\n", BLUE, test_name, NC);
    printf("%s========================================%s\n", CYAN, NC);
}

void assert_string_equals(const char* expected, const char* actual, const char* test_name) {
    stats.total++;
    if (strcmp(expected, actual) == 0) {
        printf("  %s✓%s %s\n", GREEN, NC, test_name);
        stats.passed++;
    } else {
        printf("  %s✗%s %s\n", RED, NC, test_name);
        printf("    Expected: '%s'\n", expected);
        printf("    Got:      '%s'\n", actual);
        stats.failed++;
    }
}

void test_transformation(const char* (*transform)(const char*), 
                         const char* input, 
                         const char* expected, 
                         const char* test_name) {
    const char* result = transform(input);
    if (result == NULL && expected == NULL) {
        printf("  %s✓%s %s (NULL handling)\n", GREEN, NC, test_name);
        stats.passed++;
        stats.total++;
        return;
    }
    
    if (result == NULL || expected == NULL) {
        printf("  %s✗%s %s (unexpected NULL)\n", RED, NC, test_name);
        stats.failed++;
        stats.total++;
        if (result) free((char*)result);
        return;
    }
    
    assert_string_equals(expected, result, test_name);
    free((char*)result);
}

/****************************************
 * TRANSFORMATION TESTS
 ****************************************/

void test_uppercaser_transformations() {
    print_test_header("Uppercaser Transformations");
    
    test_transformation(uppercaser_transform, "hello", "HELLO", "lowercase to uppercase");
    test_transformation(uppercaser_transform, "Hello World", "HELLO WORLD", "mixed case");
    test_transformation(uppercaser_transform, "ALREADY UPPER", "ALREADY UPPER", "already uppercase");
    test_transformation(uppercaser_transform, "123abc", "123ABC", "with numbers");
    test_transformation(uppercaser_transform, "", "", "empty string");
    test_transformation(uppercaser_transform, "!@#$%", "!@#$%", "special characters");
    test_transformation(uppercaser_transform, NULL, NULL, "NULL input");
}

void test_rotator_transformations() {
    print_test_header("Rotator Transformations");
    
    test_transformation(rotator_transform, "hello", "ohell", "basic rotation");
    test_transformation(rotator_transform, "a", "a", "single character");
    test_transformation(rotator_transform, "ab", "ba", "two characters");
    test_transformation(rotator_transform, "", "", "empty string");
    test_transformation(rotator_transform, "12345", "51234", "numbers");
    test_transformation(rotator_transform, NULL, NULL, "NULL input");
}

void test_flipper_transformations() {
    print_test_header("Flipper Transformations");
    
    test_transformation(flipper_transform, "hello", "olleh", "basic flip");
    test_transformation(flipper_transform, "a", "a", "single character");
    test_transformation(flipper_transform, "ab", "ba", "two characters");
    test_transformation(flipper_transform, "", "", "empty string");
    test_transformation(flipper_transform, "12345", "54321", "numbers");
    test_transformation(flipper_transform, NULL, NULL, "NULL input");
}

void test_expander_transformations() {
    print_test_header("Expander Transformations");
    
    test_transformation(expander_transform, "hello", "h e l l o", "basic expansion");
    test_transformation(expander_transform, "a", "a", "single character");
    test_transformation(expander_transform, "ab", "a b", "two characters");
    test_transformation(expander_transform, "", "", "empty string");
    test_transformation(expander_transform, "123", "1 2 3", "numbers");
    test_transformation(expander_transform, NULL, NULL, "NULL input");
}

/****************************************
 * PLUGIN INFRASTRUCTURE TESTS
 ****************************************/

void test_plugin_initialization() {
    print_test_header("Plugin Initialization");
    
    // Reset global context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    const char* error = common_plugin_init(uppercaser_transform, "test_uppercaser", 5);
    stats.total++;
    if (error == NULL) {
        printf("  %s✓%s Plugin initialization successful\n", GREEN, NC);
        stats.passed++;
    } else {
        printf("  %s✗%s Plugin initialization failed: %s\n", RED, NC, error);
        stats.failed++;
        return;
    }
    
    // Test plugin_get_name
    const char* name = plugin_get_name();
    assert_string_equals("test_uppercaser", name, "plugin_get_name");
    
    // Cleanup
    plugin_fini();
}

void test_plugin_workflow() {
    print_test_header("Plugin Full Workflow");
    
    // Reset global context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    // Initialize plugin
    const char* error = common_plugin_init(uppercaser_transform, "workflow_test", 3);
    stats.total++;
    if (error != NULL) {
        printf("  %s✗%s Failed to initialize: %s\n", RED, NC, error);
        stats.failed++;
        return;
    }
    printf("  %s✓%s Plugin initialized\n", GREEN, NC);
    stats.passed++;
    
    // Create a mock next plugin
    static char* last_received = NULL;
    const char* mock_next_plugin(const char* str) {
        if (last_received){ free(last_received); }
        last_received = str ? strdup(str) : NULL;
        return NULL;
    }
    
    // Attach next plugin
    plugin_attach(mock_next_plugin);
    printf("  %s✓%s Plugin attached\n", GREEN, NC);
    
    // Send work
    error = plugin_place_work("hello");
    stats.total++;
    if (error != NULL) {
        printf("  %s✗%s Failed to place work: %s\n", RED, NC, error);
        stats.failed++;
        plugin_fini();
        return;
    }
    printf("  %s✓%s Work placed\n", GREEN, NC);
    stats.passed++;
    
    // Wait a bit for processing
    usleep(100000);
    
    // Check if next plugin received uppercase
    stats.total++;
    if (last_received && strcmp(last_received, "HELLO") == 0) {
        printf("  %s✓%s Next plugin received transformed string\n", GREEN, NC);
        stats.passed++;
    } else {
        printf("  %s✗%s Next plugin didn't receive expected string\n", RED, NC);
        stats.failed++;
    }
    
    // Send END signal
    plugin_place_work("<END>");
    plugin_wait_finished();
    plugin_fini();
    
    printf("  %s✓%s Plugin shutdown complete\n", GREEN, NC);
    
    if (last_received) free(last_received);
}

void test_plugin_chaining() {
    print_test_header("Plugin Chaining Simulation");
    
    // Test a chain: uppercaser -> rotator -> flipper
    const char* input = "hello";
    
    // Step 1: Uppercaser
    const char* step1 = uppercaser_transform(input);
    assert_string_equals("HELLO", step1, "After uppercaser");
    
    // Step 2: Rotator
    const char* step2 = rotator_transform(step1);
    assert_string_equals("OHELL", step2, "After rotator");
    
    // Step 3: Flipper
    const char* step3 = flipper_transform(step2);
    assert_string_equals("LLEHO", step3, "After flipper");
    
    free((char*)step1);
    free((char*)step2);
    free((char*)step3);
}

void test_memory_stress() {
    print_test_header("Memory Stress Test");
    
    // Run many transformations to check for leaks
    for (int i = 0; i < 1000; i++) {
        char input[32];
        snprintf(input, sizeof(input), "test_%d", i);
        
        const char* result = uppercaser_transform(input);
        if (result) free((char*)result);
        
        result = rotator_transform(input);
        if (result) free((char*)result);
        
        result = flipper_transform(input);
        if (result) free((char*)result);
        
        result = expander_transform(input);
        if (result) free((char*)result);
    }
    
    printf("  %s✓%s Completed 1000 iterations without crash\n", GREEN, NC);
    stats.passed++;
    stats.total++;
}

/****************************************
 * MAIN TEST RUNNER
 ****************************************/

int main() {
    printf("%s===========================================\n", CYAN);
    printf("    PLUGIN DIRECT TESTING SUITE\n");
    printf("    (No dynamic loading required)\n");
    printf("===========================================%s\n", NC);
    
    // Test individual transformations
    test_uppercaser_transformations();
    test_rotator_transformations();
    test_flipper_transformations();
    test_expander_transformations();
    
    // Test plugin infrastructure
    test_plugin_initialization();
    test_plugin_workflow();
    test_plugin_chaining();
    test_memory_stress();
    
    // Print summary
    printf("\n%s===========================================%s\n", CYAN, NC);
    printf("                SUMMARY\n");
    printf("%s===========================================%s\n", CYAN, NC);
    printf("%sTests Passed: %d%s\n", GREEN, stats.passed, NC);
    printf("%sTests Failed: %d%s\n", RED, stats.failed, NC);
    printf("Total Tests:  %d\n", stats.total);
    
    double pass_rate = (stats.passed * 100.0) / stats.total;
    printf("Pass Rate:    %.1f%%\n", pass_rate);
    
    if (g_captured_output) free(g_captured_output);
    
    if (stats.failed > 0) {
        printf("\n%sResult: FAILURE - Some tests failed!%s\n", RED, NC);
        return 1;
    } else {
        printf("\n%sResult: SUCCESS - All tests passed!%s\n", GREEN, NC);
        return 0;
    }
}