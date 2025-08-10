#include "../plugins/sync/monitor.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define NUM_WAITERS 5
#define NUM_SIGNAL_CYCLES 10  // Reduced cycles for manual-reset testing
#define SIGNALS_PER_CYCLE 3   // Multiple signals per cycle
#define TOTAL_EXPECTED_SIGNALS (NUM_SIGNAL_CYCLES * SIGNALS_PER_CYCLE)

monitor_t test_monitor;
volatile int signals_sent = 0;
volatile int signals_received = 0;
volatile int test_running = 1;
volatile int current_cycle = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void* waiter_thread(void* arg) {
    int thread_id = *(int*)arg;
    int local_received = 0;
    
    printf("Waiter %d starting\n", thread_id);
    
    while (1) {
        // Check if test should stop BEFORE waiting
        pthread_mutex_lock(&counter_mutex);
        int should_stop = !test_running;
        pthread_mutex_unlock(&counter_mutex);
        
        if (should_stop) {
            printf("Waiter %d: stopping (test finished)\n", thread_id);
            break;
        }
        
        if (monitor_wait(&test_monitor) == 0) {
            pthread_mutex_lock(&counter_mutex);
            signals_received++;
            local_received++;
            int total_received = signals_received;
            int cycle = current_cycle;
            pthread_mutex_unlock(&counter_mutex);
            
            printf("Waiter %d: received signal (local: %d, total: %d, cycle: %d)\n", 
                   thread_id, local_received, total_received, cycle);
        } else {
            printf("Waiter %d: wait failed\n", thread_id);
            break;
        }
        
        usleep(5000); // Small delay to make output readable
    }
    
    printf("Waiter %d exiting (received %d signals)\n", thread_id, local_received);
    return NULL;
}

void* signaler_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Signaler %d starting\n", thread_id);
    
    for (int cycle = 0; cycle < NUM_SIGNAL_CYCLES; cycle++) {
        // Update current cycle
        pthread_mutex_lock(&counter_mutex);
        current_cycle = cycle + 1;
        pthread_mutex_unlock(&counter_mutex);
        
        printf("Signaler %d: Starting cycle %d\n", thread_id, cycle + 1);
        
        // Send multiple signals in this cycle
        for (int i = 0; i < SIGNALS_PER_CYCLE; i++) {
            monitor_signal(&test_monitor);
            
            pthread_mutex_lock(&counter_mutex);
            signals_sent++;
            int total_sent = signals_sent;
            pthread_mutex_unlock(&counter_mutex);
            
            printf("Signaler %d: sent signal %d in cycle %d (total sent: %d)\n", 
                   thread_id, i + 1, cycle + 1, total_sent);
            usleep(10000); // Short delay between signals in same cycle
        }
        
        // Wait a bit for waiters to process
        usleep(50000); // 50ms
        
        // Manual reset to test manual-reset behavior
        printf("Signaler %d: Resetting monitor after cycle %d\n", thread_id, cycle + 1);
        monitor_reset(&test_monitor);
        
        usleep(20000); // Wait between cycles
    }
    
    printf("Signaler %d finished\n", thread_id);
    return NULL;
}

int main() {
    pthread_t waiters[NUM_WAITERS];
    pthread_t signaler;  // Only one signaler for clearer manual-reset testing
    int waiter_ids[NUM_WAITERS];
    int signaler_id = 0;
    
    printf("Starting MANUAL-RESET monitor test:\n");
    printf("- %d waiter threads\n", NUM_WAITERS);
    printf("- 1 signaler thread\n");
    printf("- %d signal cycles\n", NUM_SIGNAL_CYCLES);
    printf("- %d signals per cycle\n", SIGNALS_PER_CYCLE);
    printf("- %d total signals\n", TOTAL_EXPECTED_SIGNALS);
    printf("- With manual-reset, multiple waiters can wake up per signal\n");
    printf("----------------------------------------\n");
    
    if (monitor_init(&test_monitor) != 0) {
        printf("ERROR: Failed to initialize monitor\n");
        return 1;
    }
    
    // Create waiter threads
    for (int i = 0; i < NUM_WAITERS; i++) {
        waiter_ids[i] = i;
        if (pthread_create(&waiters[i], NULL, waiter_thread, &waiter_ids[i]) != 0) {
            printf("ERROR: Failed to create waiter thread %d\n", i);
            return 1;
        }
    }
    
    // Small delay to let waiters start
    usleep(100000); // 100ms
    
    // Create signaler thread
    if (pthread_create(&signaler, NULL, signaler_thread, &signaler_id) != 0) {
        printf("ERROR: Failed to create signaler thread\n");
        return 1;
    }
    
    // Wait for signaler to complete
    pthread_join(signaler, NULL);
    
    printf("Signaler finished. Waiting for remaining signals to be processed...\n");
    
    // Wait a bit for remaining signals to be processed
    sleep(1);
    
    // Stop the test
    pthread_mutex_lock(&counter_mutex);
    test_running = 0;
    int final_received = signals_received;
    int final_sent = signals_sent;
    pthread_mutex_unlock(&counter_mutex);
    
    printf("Stopping test. Sending final signal to wake remaining waiters...\n");
    
    // Send a final signal to wake up any remaining waiters
    monitor_signal(&test_monitor);
    
    // Wait for all waiters to complete
    for (int i = 0; i < NUM_WAITERS; i++) {
        printf("Waiting for waiter %d to finish...\n", i);
        pthread_join(waiters[i], NULL);
    }
    
    printf("----------------------------------------\n");
    printf("Manual-Reset Monitor Test Results:\n");
    printf("- Signals sent: %d\n", final_sent);
    printf("- Signals received: %d\n", final_received);
    printf("- Ratio: %.2f (received/sent)\n", (float)final_received / final_sent);
    
    // For manual-reset, we expect received >= sent because multiple waiters
    // can wake up from a single signal
    if (final_received >= TOTAL_EXPECTED_SIGNALS) {
        printf("✅ SUCCESS: Manual-reset behavior working correctly!\n");
        printf("   (Multiple waiters can wake up from single signal)\n");
    } else {
        printf("❌ FAILURE: Expected at least %d, got %d signals\n", 
               TOTAL_EXPECTED_SIGNALS, final_received);
    }
    
    monitor_destroy(&test_monitor);
    pthread_mutex_destroy(&counter_mutex);
    return (final_received >= TOTAL_EXPECTED_SIGNALS) ? 0 : 1;
}