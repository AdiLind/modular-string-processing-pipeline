#define _GNU_SOURCE
#include "plugins/sync/consumer_producer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const char* test_string = "test_direct";
    size_t len = strlen(test_string);
    queue.items[0] = (char*)malloc(len + 1);
    if (queue.items[0]) {
        strcpy(queue.items[0], test_string);
    }
    queue.count = 1;
    queue.head = 0;
    queue.tail = 1;
    
    printf("Direct assignment done. About to read...\n");
    printf("queue.items[0] = %p\n", (void*)queue.items[0]);
    
    if (queue.items[0]) {
        printf("Direct read successful: %s\n", queue.items[0]);
    }
    
    printf("About to destroy queue...\n");
    fflush(stdout);
    consumer_producer_destroy(&queue);
    printf("Queue destroyed!\n");
    
    printf("=== MINIMAL TEST COMPLETE ===\n");
}

int main() {
    printf("Starting simple plugin infrastructure test...\n");
    minimal_queue_test();
    printf("Test completed successfully!\n");
    return 0;
}