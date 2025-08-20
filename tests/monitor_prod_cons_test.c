// monitor_consumer_producer_test.c
// A comprehensive test suite for the monitor and consumer-producer queue.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "../plugins/sync/monitor.h"
#include "../plugins/sync/consumer_producer.h"

// =============================================================================
// Test Utilities
// =============================================================================

#define TEST_PASSED() printf("  \033[0;32mPASSED\033[0m\n")
#define TEST_FAILED(reason) printf("  \033[0;31mFAILED\033[0m: %s\n", reason)

typedef struct {
    monitor_t* monitor;
    int* flag;
} thread_arg_t;

// =============================================================================
// Monitor Tests
// =============================================================================

void* wait_then_signal_thread(void* arg) {
    thread_arg_t* data = (thread_arg_t*)arg;
    usleep(100 * 1000); // 100ms
    *(data->flag) = 1; // Mark that we are about to signal
    monitor_signal(data->monitor);
    return NULL;
}

void test_monitor_signal_before_wait() {
    printf("1. test_monitor_signal_before_wait:\n");
    monitor_t m;
    assert(monitor_init(&m) == 0);

    // Signal before anyone is waiting
    monitor_signal(&m);
    
    // Now wait. Should not block because the signal was "remembered".
    int result = monitor_wait(&m);

    if (result == 0) {
        TEST_PASSED();
    } else {
        TEST_FAILED("monitor_wait should have returned immediately.");
    }
    monitor_destroy(&m);
}

void test_monitor_wait_before_signal() {
    printf("2. test_monitor_wait_before_signal:\n");
    monitor_t m;
    pthread_t tid;
    int flag = 0;
    thread_arg_t args = {&m, &flag};
    
    assert(monitor_init(&m) == 0);
    
    pthread_create(&tid, NULL, wait_then_signal_thread, &args);
    
    // Wait should block until the other thread signals.
    monitor_wait(&m);
    
    // By the time wait returns, the flag should have been set by the other thread.
    if (flag == 1) {
        TEST_PASSED();
    } else {
        TEST_FAILED("monitor_wait returned but flag was not set.");
    }
    
    pthread_join(tid, NULL);
    monitor_destroy(&m);
}
void test_monitor_signal_consumption() {
    printf("3. test_monitor_manual_reset_behavior:\n");
    monitor_t m;
    pthread_t tid1, tid2;
    int flag1 = 0, flag2 = 0;
    thread_arg_t args1 = {&m, &flag1};
    thread_arg_t args2 = {&m, &flag2};
    
    assert(monitor_init(&m) == 0);
    
    // Signal once
    monitor_signal(&m);
    
    // First wait should succeed immediately
    monitor_wait(&m);
    
    // MANUAL-RESET: Signal should still be set!
    // Second wait should also succeed immediately
    monitor_wait(&m);
    
    // Both waits succeeded because signal persisted (manual-reset behavior)
    printf("  Manual-reset behavior verified: signal persisted\n");
    
    // Now test that reset actually clears the signal
    monitor_reset(&m);
    
    // After reset, wait should block
    pthread_create(&tid1, NULL, wait_then_signal_thread, &args1);
    
    monitor_wait(&m); // Should block until thread signals
    
    if (flag1 == 1) {
        TEST_PASSED();
    } else {
        TEST_FAILED("Wait did not block after reset.");
    }
    
    pthread_join(tid1, NULL);
    monitor_destroy(&m);
}

// =============================================================================
// Consumer-Producer Queue Tests
// =============================================================================

#define NUM_ITEMS 100
#define QUEUE_CAPACITY 10
#define NUM_PRODUCERS 4
#define NUM_CONSUMERS 4

typedef struct {
    consumer_producer_t* queue;
    int producer_id;
    int num_items;
    int* produced_count;
} producer_args;

typedef struct {
    consumer_producer_t* queue;
    int* consumed_count;
    pthread_mutex_t* count_mutex;
} consumer_args;

void* producer_thread(void* arg) {
    producer_args* args = (producer_args*)arg;
    for (int i = 0; i < args->num_items; ++i) {
        char buffer[50];
        sprintf(buffer, "p%d_item%d", args->producer_id, i);
        
        // We need to pass a heap-allocated string, as the queue takes ownership.
        char* item = strdup(buffer);
        assert(item != NULL);
        
        consumer_producer_put(args->queue, item);
        (*(args->produced_count))++;
    }
    return NULL;
}

void* consumer_thread(void* arg) {
    consumer_args* args = (consumer_args*)arg;
    while(1) {
        char* item = consumer_producer_get(args->queue);
        if (strcmp(item, "END") == 0) {
            // Put the sentinel back for other consumers
            consumer_producer_put(args->queue, item);
            break;
        }
        
        pthread_mutex_lock(args->count_mutex);
        (*(args->consumed_count))++;
        pthread_mutex_unlock(args->count_mutex);
        
        // Free the memory allocated by producer
        free(item);
    }
    return NULL;
}

void test_queue_fifo_order() {
    printf("4. test_queue_fifo_order:\n");
    consumer_producer_t q;
    assert(consumer_producer_init(&q, 5) == NULL);

    consumer_producer_put(&q, strdup("item1"));
    consumer_producer_put(&q, strdup("item2"));
    consumer_producer_put(&q, strdup("item3"));

    char* item1 = consumer_producer_get(&q);
    char* item2 = consumer_producer_get(&q);
    char* item3 = consumer_producer_get(&q);

    if (strcmp(item1, "item1") == 0 && strcmp(item2, "item2") == 0 && strcmp(item3, "item3") == 0) {
        TEST_PASSED();
    } else {
        TEST_FAILED("Items were not retrieved in FIFO order.");
    }

    free(item1);
    free(item2);
    free(item3);
    consumer_producer_destroy(&q);
}

void test_consumer_blocks_on_empty() {
    printf("5. test_consumer_blocks_on_empty:\n");
    consumer_producer_t q;
    pthread_t consumer_tid;
    int consumed_count = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    consumer_args args = {&q, &consumed_count, &mutex};

    assert(consumer_producer_init(&q, 5) == NULL);
    
    // This consumer will call 'get' and block.
    pthread_create(&consumer_tid, NULL, consumer_thread, &args);

    usleep(200 * 1000); // Give consumer time to block
    
    // Check that it's blocked (consumed_count is still 0)
    assert(consumed_count == 0); 
    
    consumer_producer_put(&q, strdup("data"));
    consumer_producer_put(&q, strdup("END")); // Sentinel to stop consumer

    pthread_join(consumer_tid, NULL);
    
    if (consumed_count == 1) {
        TEST_PASSED();
    } else {
        TEST_FAILED("Consumer did not seem to block and wake up correctly.");
    }
    
    // Clean up remaining END message
    char* end_msg = consumer_producer_get(&q);
    free(end_msg);
    consumer_producer_destroy(&q);
    pthread_mutex_destroy(&mutex);
}

void test_producer_blocks_on_full() {
    printf("6. test_producer_blocks_on_full:\n");
    consumer_producer_t q;
    pthread_t producer_tid;
    int produced_count = 0;
    
    // Capacity of 1 is a great edge case
    assert(consumer_producer_init(&q, 1) == NULL);
    producer_args args = {&q, 1, 1, &produced_count};

    char* item_copy = strdup("item1");
    consumer_producer_put(&q, item_copy);
    assert(produced_count == 0); // Local counter not used for first item
    
    // This producer should block because the queue is full.
    pthread_create(&producer_tid, NULL, producer_thread, &args);
    
    usleep(200 * 1000); // Give producer time to block
    
    // Check that it's blocked (produced_count is still 0)
    assert(produced_count == 0);
    
    char* item = consumer_producer_get(&q); // Make space
    free(item);

    pthread_join(producer_tid, NULL);

    if (produced_count == 1) {
        TEST_PASSED();
    } else {
        TEST_FAILED("Producer did not seem to block and wake up correctly.");
    }
    
    item = consumer_producer_get(&q);
    free(item);
    consumer_producer_destroy(&q);
}

void test_multi_producer_multi_consumer() {
    printf("7. test_multi_producer_multi_consumer:\n");
    consumer_producer_t q;
    pthread_t p_threads[NUM_PRODUCERS];
    pthread_t c_threads[NUM_CONSUMERS];
    
    int total_produced = 0;
    int total_consumed = 0;
    pthread_mutex_t consume_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    assert(consumer_producer_init(&q, QUEUE_CAPACITY) == NULL);

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producer_args* args = malloc(sizeof(producer_args));
        args->queue = &q;
        args->producer_id = i;
        args->num_items = NUM_ITEMS;
        args->produced_count = &total_produced; // Not thread safe, just a rough check
        pthread_create(&p_threads[i], NULL, producer_thread, args);
    }
    
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumer_args* args = malloc(sizeof(consumer_args));
        args->queue = &q;
        args->consumed_count = &total_consumed;
        args->count_mutex = &consume_mutex;
        pthread_create(&c_threads[i], NULL, consumer_thread, args);
    }
    
    // Wait for all producers to finish
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(p_threads[i], NULL);
    }
    
    // Add sentinel values for all consumers
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumer_producer_put(&q, strdup("END"));
    }
    
    // Wait for all consumers to finish
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(c_threads[i], NULL);
    }

    int expected_items = NUM_PRODUCERS * NUM_ITEMS;
    if (total_consumed == expected_items) {
        TEST_PASSED();
    } else {
        char reason[100];
        sprintf(reason, "Expected %d items, but consumers got %d.", expected_items, total_consumed);
        TEST_FAILED(reason);
    }
    
    // Clean up any remaining END messages
    char* msg;
    while(q.count > 0) {
        msg = consumer_producer_get(&q);
        free(msg);
    }
    
    consumer_producer_destroy(&q);
    pthread_mutex_destroy(&consume_mutex);
}


// =============================================================================
// Main
// =============================================================================

int main() {
    printf("--- Running Monitor Tests ---\n");
    test_monitor_signal_before_wait();
    test_monitor_wait_before_signal();
    test_monitor_signal_consumption();

    printf("\n--- Running Consumer-Producer Queue Tests ---\n");
    test_queue_fifo_order();
    test_consumer_blocks_on_empty();
    test_producer_blocks_on_full();
    test_multi_producer_multi_consumer();
    
    printf("\nAll tests complete.\n");
    
    return 0;
}