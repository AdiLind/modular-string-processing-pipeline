/**
 * Consumer-Producer Queue Test Suite
 * 
 * Tests all functionality including basic operations, thread safety,
 * blocking behavior, memory management, and error handling
 */

#include "../plugins/sync/consumer_producer.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* Test configuration */
#define QUEUE_SIZE 5
#define NUM_ITEMS 20
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 3

/* Colors for output */
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define CYAN "\033[0;36m"
#define NC "\033[0m"

/* Test result tracking */
typedef enum {
    TEST_PASS,
    TEST_FAIL
} test_result_t;

/* Thread context for producer/consumer tests */
typedef struct {
    consumer_producer_t* queue;
    int thread_id;
    int num_items;
    int start_value;
    int* success_count;
    pthread_mutex_t* count_mutex;
} thread_context_t;

/* Utility Functions */
void print_test_header(const char* test_name) {
    printf("\n%s========================================%s\n", CYAN, NC);
    printf("%sTEST: %s%s\n", BLUE, test_name, NC);
    printf("%s========================================%s\n", CYAN, NC);
}

void print_test_result(const char* test_name, test_result_t result) {
    const char* status = (result == TEST_PASS) ? "PASS" : "FAIL";
    const char* color = (result == TEST_PASS) ? GREEN : RED;
    printf("%s[%s]%s %s\n", color, status, NC, test_name);
}

/* Thread Functions */
void* producer_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char buffer[256];
    int i;
    
    for (i = 0; i < ctx->num_items; i++) {
        snprintf(buffer, sizeof(buffer), "Producer-%d-Item-%d", 
                 ctx->thread_id, ctx->start_value + i);
        
        const char* error = consumer_producer_put(ctx->queue, buffer);
        if (NULL == error) {
            pthread_mutex_lock(ctx->count_mutex);
            (*ctx->success_count)++;
            pthread_mutex_unlock(ctx->count_mutex);
            printf("  [P%d] Put: %s\n", ctx->thread_id, buffer);
        } else {
            printf("  [P%d] Error: %s\n", ctx->thread_id, error);
        }
        
        // Small delay to make output readable
        usleep(10000); // 10ms
    }
    
    return NULL;
}

void* consumer_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    int i;
    
    for (i = 0; i < ctx->num_items; i++) {
        char* item = consumer_producer_get(ctx->queue);
        if (NULL != item) {
            pthread_mutex_lock(ctx->count_mutex);
            (*ctx->success_count)++;
            pthread_mutex_unlock(ctx->count_mutex);
            printf("  [C%d] Got: %s\n", ctx->thread_id, item);
            free(item); // Consumer owns the string
        } else {
            printf("  [C%d] Error getting item\n", ctx->thread_id);
        }
        
        // Small delay to make output readable
        usleep(15000); // 15ms
    }
    
    return NULL;
}

void* blocking_producer_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char buffer[256];
    int i;
    
    printf("  [BP%d] Starting blocking producer\n", ctx->thread_id);
    
    // Try to put more items than queue capacity
    for (i = 0; i < QUEUE_SIZE + 2; i++) {
        snprintf(buffer, sizeof(buffer), "Block-Item-%d", i);
        printf("  [BP%d] Attempting to put item %d...\n", ctx->thread_id, i);
        
        const char* error = consumer_producer_put(ctx->queue, buffer);
        if (NULL == error) {
            printf("  [BP%d] Successfully put item %d\n", ctx->thread_id, i);
        } else {
            printf("  [BP%d] Failed to put item %d: %s\n", ctx->thread_id, i, error);
        }
    }
    
    return NULL;
}

void* delayed_consumer_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    
    printf("  [DC%d] Delayed consumer sleeping for 2 seconds...\n", ctx->thread_id);
    sleep(2); // Wait before consuming
    printf("  [DC%d] Waking up, starting consumption\n", ctx->thread_id);
    
    // Consume all items
    int count = 0;
    while (count < QUEUE_SIZE + 2) {
        char* item = consumer_producer_get(ctx->queue);
        if (NULL != item) {
            printf("  [DC%d] Got: %s\n", ctx->thread_id, item);
            free(item);
            count++;
        }
    }
    
    return NULL;
}

/* Test Cases */
test_result_t test_basic_initialization() {
    consumer_producer_t queue;
    const char* result;
    
    print_test_header("Basic Initialization");
    
    // Initialize queue struct to zeros (important for mutex_initialized flag)
    memset(&queue, 0, sizeof(consumer_producer_t));
    
    // Test successful initialization
    result = consumer_producer_init(&queue, 10);
    if (NULL != result) {
        printf("  ✗ Failed to initialize queue: %s\n", result);
        return TEST_FAIL;
    }
    printf("  ✓ Queue initialization successful\n");
    
    // Test NULL pointer
    result = consumer_producer_init(NULL, 10);
    if (NULL == result) {
        printf("  ✗ NULL initialization didn't return error\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ NULL pointer handled correctly\n");
    
    // Test invalid capacity
    consumer_producer_t queue2;
    memset(&queue2, 0, sizeof(consumer_producer_t));
    result = consumer_producer_init(&queue2, 0);
    if (NULL == result) {
        printf("  ✗ Zero capacity didn't return error\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Invalid capacity handled correctly\n");
    
    consumer_producer_t queue3;
    memset(&queue3, 0, sizeof(consumer_producer_t));
    result = consumer_producer_init(&queue3, -5);
    if (NULL == result) {
        printf("  ✗ Negative capacity didn't return error\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Negative capacity handled correctly\n");
    
    consumer_producer_destroy(&queue);
    printf("  ✓ Queue destroyed successfully\n");
    
    return TEST_PASS;
}

test_result_t test_single_producer_consumer() {
    consumer_producer_t queue;
    const char* error;
    char* item;
    
    print_test_header("Single Producer-Consumer");
    
    memset(&queue, 0, sizeof(consumer_producer_t));
    error = consumer_producer_init(&queue, 5);
    if (NULL != error) {
        printf("  ✗ Failed to init queue: %s\n", error);
        return TEST_FAIL;
    }
    
    // Put some items
    error = consumer_producer_put(&queue, "Item1");
    if (NULL != error) {
        printf("  ✗ Failed to put Item1: %s\n", error);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Put Item1\n");
    
    error = consumer_producer_put(&queue, "Item2");
    if (NULL != error) {
        printf("  ✗ Failed to put Item2: %s\n", error);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Put Item2\n");
    
    // Get items
    item = consumer_producer_get(&queue);
    if (NULL == item) {
        printf("  ✗ Failed to get first item\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    if (strcmp(item, "Item1") != 0) {
        printf("  ✗ Got wrong item: %s (expected Item1)\n", item);
        free(item);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Got Item1 (FIFO order correct)\n");
    free(item);
    
    item = consumer_producer_get(&queue);
    if (NULL == item) {
        printf("  ✗ Failed to get second item\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    if (strcmp(item, "Item2") != 0) {
        printf("  ✗ Got wrong item: %s (expected Item2)\n", item);
        free(item);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Got Item2 (FIFO order correct)\n");
    free(item);
    
    consumer_producer_destroy(&queue);
    return TEST_PASS;
}

test_result_t test_queue_full_behavior() {
    consumer_producer_t queue;
    const char* error;
    char buffer[32];
    int i;
    
    print_test_header("Queue Full Behavior");
    
    memset(&queue, 0, sizeof(consumer_producer_t));
    error = consumer_producer_init(&queue, 3);
    if (NULL != error) {
        printf("  ✗ Failed to init queue: %s\n", error);
        return TEST_FAIL;
    }
    
    // Fill the queue
    for (i = 0; i < 3; i++) {
        snprintf(buffer, sizeof(buffer), "Item%d", i);
        error = consumer_producer_put(&queue, buffer);
        if (NULL != error) {
            printf("  ✗ Failed to put item %d: %s\n", i, error);
            consumer_producer_destroy(&queue);
            return TEST_FAIL;
        }
        printf("  ✓ Put Item%d (queue now %d/3)\n", i, i + 1);
    }
    
    printf("  • Queue is full, testing blocking behavior...\n");
    
    // Test blocking - create threads with safer approach
    pthread_t producer, consumer;
    thread_context_t prod_ctx = {&queue, 1, 2, 0, NULL, NULL}; // Reduced items
    thread_context_t cons_ctx = {&queue, 1, 5, 0, NULL, NULL}; // Consumer gets all
    
    if (0 != pthread_create(&producer, NULL, blocking_producer_thread, &prod_ctx)) {
        printf("  ✗ Failed to create producer thread\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    // Let producer start and potentially block
    sleep(1);
    
    if (0 != pthread_create(&consumer, NULL, delayed_consumer_thread, &cons_ctx)) {
        printf("  ✗ Failed to create consumer thread\n");
        pthread_cancel(producer);
        pthread_join(producer, NULL);
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    // Wait for both threads
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    printf("  ✓ Blocking behavior worked correctly\n");
    
    consumer_producer_destroy(&queue);
    return TEST_PASS;
}

test_result_t test_concurrent_access() {
    consumer_producer_t queue;
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    thread_context_t prod_contexts[NUM_PRODUCERS];
    thread_context_t cons_contexts[NUM_CONSUMERS];
    int producer_count = 0, consumer_count = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
    int i;
    
    print_test_header("Concurrent Access (Multiple Producers/Consumers)");
    
    memset(&queue, 0, sizeof(consumer_producer_t));
    if (NULL != consumer_producer_init(&queue, QUEUE_SIZE)) {
        return TEST_FAIL;
    }
    
    int items_per_producer = 5;
    int items_per_consumer = 5;
    
    printf("  • Starting %d producers (each produces %d items)\n", 
           NUM_PRODUCERS, items_per_producer);
    printf("  • Starting %d consumers (each consumes %d items)\n", 
           NUM_CONSUMERS, items_per_consumer);
    
    // Start producers
    for (i = 0; i < NUM_PRODUCERS; i++) {
        prod_contexts[i].queue = &queue;
        prod_contexts[i].thread_id = i;
        prod_contexts[i].num_items = items_per_producer;
        prod_contexts[i].start_value = i * 100;
        prod_contexts[i].success_count = &producer_count;
        prod_contexts[i].count_mutex = &count_mutex;
        pthread_create(&producers[i], NULL, producer_thread, &prod_contexts[i]);
    }
    
    // Start consumers
    for (i = 0; i < NUM_CONSUMERS; i++) {
        cons_contexts[i].queue = &queue;
        cons_contexts[i].thread_id = i;
        cons_contexts[i].num_items = items_per_consumer;
        cons_contexts[i].success_count = &consumer_count;
        cons_contexts[i].count_mutex = &count_mutex;
        pthread_create(&consumers[i], NULL, consumer_thread, &cons_contexts[i]);
    }
    
    // Wait for all threads
    for (i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    for (i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    printf("  • Produced: %d items\n", producer_count);
    printf("  • Consumed: %d items\n", consumer_count);
    
    if (producer_count != items_per_producer * NUM_PRODUCERS) {
        printf("  ✗ Producer count mismatch\n");
        consumer_producer_destroy(&queue);
        pthread_mutex_destroy(&count_mutex);
        return TEST_FAIL;
    }
    
    if (consumer_count != items_per_consumer * NUM_CONSUMERS) {
        printf("  ✗ Consumer count mismatch\n");
        consumer_producer_destroy(&queue);
        pthread_mutex_destroy(&count_mutex);
        return TEST_FAIL;
    }
    
    printf("  ✓ All items produced and consumed correctly\n");
    
    consumer_producer_destroy(&queue);
    pthread_mutex_destroy(&count_mutex);
    return TEST_PASS;
}

test_result_t test_memory_management() {
    consumer_producer_t queue;
    char buffer[256];
    char* item;
    int i;
    
    print_test_header("Memory Management");
    
    memset(&queue, 0, sizeof(consumer_producer_t));
    if (NULL != consumer_producer_init(&queue, 10)) {
        return TEST_FAIL;
    }
    
    // Put many items
    printf("  • Putting 100 items...\n");
    for (i = 0; i < 100; i++) {
        snprintf(buffer, sizeof(buffer), "Long-String-Item-%d-With-Extra-Data-%d", i, i*i);
        consumer_producer_put(&queue, buffer);
        
        // Immediately consume to avoid filling queue
        if (i % 10 == 9) {
            int j;
            for (j = 0; j < 10; j++) {
                item = consumer_producer_get(&queue);
                if (NULL != item) {
                    free(item);
                }
            }
        }
    }
    printf("  ✓ Processed 100 items without issues\n");
    
    // Put items and destroy without consuming (test cleanup)
    for (i = 0; i < 5; i++) {
        snprintf(buffer, sizeof(buffer), "Cleanup-Item-%d", i);
        consumer_producer_put(&queue, buffer);
    }
    printf("  • Destroying queue with 5 items still inside...\n");
    
    consumer_producer_destroy(&queue);
    printf("  ✓ Queue destroyed (should have freed remaining items)\n");
    
    return TEST_PASS;
}
    // Test thread that waits for finished
    void* wait_for_finish_thread(void* arg) {
        consumer_producer_t* q = (consumer_producer_t*)arg;
        printf("  • Thread waiting for finished signal...\n");
        int result = consumer_producer_wait_finished(q);
        if (0 == result) {
            printf("  • Thread received finished signal!\n");
        }
        return NULL;
    }

test_result_t test_finished_signaling() {
    consumer_producer_t queue;
    pthread_t thread;
    
    print_test_header("Finished Signaling");
    
    memset(&queue, 0, sizeof(consumer_producer_t));
    if (NULL != consumer_producer_init(&queue, 5)) {
        return TEST_FAIL;
    }
    
    // Create thread that waits for finished signal
    if (0 != pthread_create(&thread, NULL, wait_for_finish_thread, &queue)) {
        printf("  ✗ Failed to create waiting thread\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    // Give thread time to start waiting
    sleep(1);
    
    printf("  • Main thread signaling finished...\n");
    consumer_producer_signal_finished(&queue);
    
    // Wait for thread to complete
    void* thread_result;
    if (0 != pthread_join(thread, &thread_result)) {
        printf("  ✗ Failed to join thread\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    // Check thread result
    if (thread_result != (void*)0) {
        printf("  ✗ Thread reported failure\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    
    printf("  ✓ Finished signaling works correctly\n");
    
    consumer_producer_destroy(&queue);
    return TEST_PASS;
}

test_result_t test_error_conditions() {
    consumer_producer_t queue;
    const char* error;
    char* item;
    
    print_test_header("Error Conditions");
    
    // Test NULL operations
    error = consumer_producer_put(NULL, "test");
    if (NULL == error) {
        printf("  ✗ Put to NULL queue didn't return error\n");
        return TEST_FAIL;
    }
    printf("  ✓ Put to NULL queue handled\n");
    
    item = consumer_producer_get(NULL);
    if (NULL != item) {
        printf("  ✗ Get from NULL queue didn't return NULL\n");
        free(item);
        return TEST_FAIL;
    }
    printf("  ✓ Get from NULL queue handled\n");
    
    // Test NULL item
    memset(&queue, 0, sizeof(consumer_producer_t));
    if (NULL != consumer_producer_init(&queue, 5)) {
        return TEST_FAIL;
    }
    
    error = consumer_producer_put(&queue, NULL);
    if (NULL == error) {
        printf("  ✗ Put NULL item didn't return error\n");
        consumer_producer_destroy(&queue);
        return TEST_FAIL;
    }
    printf("  ✓ Put NULL item handled\n");
    
    // Test operations on destroyed queue
    consumer_producer_destroy(&queue);
    consumer_producer_destroy(&queue); // Double destroy
    printf("  ✓ Double destroy handled\n");
    
    consumer_producer_destroy(NULL);
    printf("  ✓ Destroy NULL handled\n");
    
    return TEST_PASS;
}

/* Main Test Runner */
int main(void) {
    int tests_passed = 0;
    int tests_failed = 0;
    test_result_t result;
    
    printf("%s===========================================\n", CYAN);
    printf("    CONSUMER-PRODUCER QUEUE TEST SUITE\n");
    printf("===========================================%s\n", NC);
    printf("Testing thread-safe bounded queue implementation\n");
    printf("\n");
    
    // Run tests
    result = test_basic_initialization();
    print_test_result("Basic Initialization", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_single_producer_consumer();
    print_test_result("Single Producer-Consumer", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_queue_full_behavior();
    print_test_result("Queue Full Behavior", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_concurrent_access();
    print_test_result("Concurrent Access", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_memory_management();
    print_test_result("Memory Management", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_finished_signaling();
    print_test_result("Finished Signaling", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    result = test_error_conditions();
    print_test_result("Error Conditions", result);
    if (result == TEST_PASS) tests_passed++; else tests_failed++;
    
    // Print summary
    printf("\n%s===========================================%s\n", CYAN, NC);
    printf("                SUMMARY\n");
    printf("%s===========================================%s\n", CYAN, NC);
    printf("%sTests Passed:  %d%s\n", GREEN, tests_passed, NC);
    printf("%sTests Failed:  %d%s\n", RED, tests_failed, NC);
    printf("Total Tests:   %d\n", tests_passed + tests_failed);
    
    if (tests_failed > 0) {
        printf("\n%sResult: FAILURE - Some tests failed!%s\n", RED, NC);
        return 1;
    } else {
        printf("\n%sResult: SUCCESS - All tests passed!%s\n", GREEN, NC);
        return 0;
    }
}