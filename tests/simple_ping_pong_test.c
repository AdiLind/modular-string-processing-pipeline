/**
 * Simple Ping-Pong Test to verify monitor functionality
 * Run this to ensure the basic ping-pong logic works
 */

#include "../plugins/sync/monitor.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    monitor_t* my_monitor;
    monitor_t* other_monitor;
    int thread_id;
    int operations;
} ping_pong_ctx_t;

void* ping_pong_simple(void* arg) {
    ping_pong_ctx_t* ctx = (ping_pong_ctx_t*)arg;
    
    printf("Thread %d starting\n", ctx->thread_id);
    
    for (int i = 0; i < 3; i++) {
        printf("Thread %d: waiting for turn %d\n", ctx->thread_id, i);
        
        // Wait for my turn
        if (monitor_wait(ctx->my_monitor) != 0) {
            printf("Thread %d: wait failed!\n", ctx->thread_id);
            return NULL;
        }
        
        printf("Thread %d: got turn %d\n", ctx->thread_id, i);
        ctx->operations++;
        
        // Reset my monitor
        monitor_reset(ctx->my_monitor);
        
        // Signal other thread
        monitor_signal(ctx->other_monitor);
        printf("Thread %d: signaled other thread\n", ctx->thread_id);
        
        // Small delay
        usleep(10000); // 10ms
    }
    
    printf("Thread %d finished with %d operations\n", ctx->thread_id, ctx->operations);
    return NULL;
}

int main() {
    monitor_t monitors[2];
    pthread_t threads[2];
    ping_pong_ctx_t contexts[2];
    
    printf("=== Simple Ping-Pong Test ===\n");
    
    // Initialize monitors
    if (monitor_init(&monitors[0]) != 0 || monitor_init(&monitors[1]) != 0) {
        printf("Failed to initialize monitors\n");
        return 1;
    }
    
    // Setup contexts
    contexts[0].my_monitor = &monitors[0];
    contexts[0].other_monitor = &monitors[1];
    contexts[0].thread_id = 0;
    contexts[0].operations = 0;
    
    contexts[1].my_monitor = &monitors[1];
    contexts[1].other_monitor = &monitors[0];
    contexts[1].thread_id = 1;
    contexts[1].operations = 0;
    
    // Start with thread 0
    monitor_signal(&monitors[0]);
    
    // Create threads
    if (pthread_create(&threads[0], NULL, ping_pong_simple, &contexts[0]) != 0) {
        printf("Failed to create thread 0\n");
        return 1;
    }
    
    if (pthread_create(&threads[1], NULL, ping_pong_simple, &contexts[1]) != 0) {
        printf("Failed to create thread 1\n");
        pthread_cancel(threads[0]);
        return 1;
    }
    
    // Wait for completion (with timeout check)
    printf("Waiting for threads to complete...\n");
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 10; // 10 second timeout
    
    int result0 = pthread_timedjoin_np(threads[0], NULL, &timeout);
    int result1 = pthread_timedjoin_np(threads[1], NULL, &timeout);
    
    if (result0 != 0 || result1 != 0) {
        printf("TIMEOUT: Threads did not complete in 10 seconds - likely deadlock\n");
        pthread_cancel(threads[0]);
        pthread_cancel(threads[1]);
        pthread_join(threads[0], NULL);
        pthread_join(threads[1], NULL);
        monitor_destroy(&monitors[0]);
        monitor_destroy(&monitors[1]);
        return 1;
    }
    
    // Check results
    printf("\nResults:\n");
    printf("Thread 0 operations: %d\n", contexts[0].operations);
    printf("Thread 1 operations: %d\n", contexts[1].operations);
    
    if (contexts[0].operations == 3 && contexts[1].operations == 3) {
        printf("SUCCESS: Ping-pong completed correctly!\n");
    } else {
        printf("FAILURE: Expected 3 operations each\n");
        monitor_destroy(&monitors[0]);
        monitor_destroy(&monitors[1]);
        return 1;
    }
    
    // Cleanup
    monitor_destroy(&monitors[0]);
    monitor_destroy(&monitors[1]);
    
    printf("=== Test Complete ===\n");
    return 0;
}