#define _GNU_SOURCE
#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


// static function declaration
static void cleanup_partial_init(consumer_producer_t* queue, int stage);


const char* consumer_producer_init(consumer_producer_t* queue, int capacity)
{
    if (NULL == queue)
    {
        return "Queue pointer is NULL";
    }

    if(capacity <= 0)
    {
        return "Invalid capacity";
    }

    memset(queue, 0, sizeof(consumer_producer_t));

    // initialize the queue
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->items = NULL;
    queue->mutex_initialized = 0; 

    queue->items = (char**)calloc(capacity, sizeof(char*));
    if (NULL == queue->items) 
    {
        return "Failed to allocate memory for items";
    }
    
    // init mutex and handle peaceful destruction 
    if(0 != pthread_mutex_init(&queue->queue_mutex, NULL))
    {
        cleanup_partial_init(queue, 0);
        return "Failed to initialize queue mutex";
    }
    queue->mutex_initialized = 1;

    //init non_full_monitor
    if(0 != monitor_init(&queue->not_full_monitor))
    {
        cleanup_partial_init(queue, 1);
        return "Failed to initialize not_full_monitor";
    }
    
    //init not_empty_monitor
    if(0 != monitor_init(&queue->not_empty_monitor))
    {
        cleanup_partial_init(queue, 2);
        return "Failed to initialize not_empty_monitor";
    }
    
    //init finished_monitor
    if(0 != monitor_init(&queue->finished_monitor))
    {
        cleanup_partial_init(queue, 3);
        return "Failed to initialize finished_monitor";
    }

    monitor_signal(&queue->not_full_monitor); // signal that queue is not full
    return NULL; // success
}

void consumer_producer_destroy(consumer_producer_t* queue) {
    if (NULL == queue)
    {
        return;
    }

    monitor_signal(&queue->not_full_monitor);
    monitor_signal(&queue->not_empty_monitor);
    monitor_signal(&queue->finished_monitor);

    if(queue->mutex_initialized) //lock the mutex before destroying
    {
        pthread_mutex_lock(&queue->queue_mutex);
    }

    // free the items in the queue
    if (NULL != queue->items) 
    {
        for (int i = 0; i < queue->capacity; i++) {
            if (NULL != queue->items[i]) {
                free(queue->items[i]);
                queue->items[i] = NULL;
            }
        }
        free(queue->items);
        queue->items = NULL;
    }

    if (queue->mutex_initialized) {
        pthread_mutex_unlock(&queue->queue_mutex);
    }

    // destroy the synchronization monitors
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);

    // destroy the mutex
    if (queue->mutex_initialized) {
        pthread_mutex_destroy(&queue->queue_mutex);
        queue->mutex_initialized = 0;
    }

    //reset the remining fields
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
}

/* 
*   mind roadmap before implementing this function: (TODO: delete before submitting)
    the critical section in this method are-
    1. reading the queue state (count, capacity)
    2. modify the queue state
    3. adding the item to the queue
    and we need to release the mutex while we are waiting and the operation should be atomic
*/

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    if (NULL == queue || NULL == item) {
        return "Queue or item pointer is NULL";
    }

    if (NULL == queue->items) {
        return "Queue items array is not initialized";
    }
    
    while (1) {
        pthread_mutex_lock(&queue->queue_mutex);
        
        if (queue->count < queue->capacity) {
            size_t len = strlen(item);
            char* copy_of_item = (char*)malloc(len + 1);
            if (NULL == copy_of_item) {
                pthread_mutex_unlock(&queue->queue_mutex);
                return "Failed to copy item string";
            }
            strcpy(copy_of_item, item);
            
            //Add item
            queue->items[queue->tail] = copy_of_item;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;
            
            pthread_mutex_unlock(&queue->queue_mutex);
            
            //asignal that queue is not empty (wake up consumers)
            monitor_signal(&queue->not_empty_monitor);
            
            return NULL;
        }
        
        // Condition not met - prepare to wait
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Wait for condition to change
        monitor_reset(&queue->not_full_monitor);
        if (0 != monitor_wait(&queue->not_full_monitor)) {
            return "Failed to wait for not_full condition";
        }
        
        //loop and try again
    }
}


char* consumer_producer_get(consumer_producer_t* queue) {
    if (NULL == queue) {
        return NULL;
    }
    
    if (NULL == queue->items) {
        return NULL;
    }

    
    while (1) {
        pthread_mutex_lock(&queue->queue_mutex);

        
        // Check condition while holding lock
        if (queue->count > 0) {

            // Items available - perform operation
            char* item = queue->items[queue->head];
            queue->items[queue->head] = NULL;
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;
            
            // Release lock before signaling
            pthread_mutex_unlock(&queue->queue_mutex);
            
            // Signal that queue is not full (wake up producers)
            monitor_signal(&queue->not_full_monitor);
            
            return item; // Success
        }
        
        // Condition not met - prepare to wait
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Wait for condition to change
        monitor_reset(&queue->not_empty_monitor);
        if (0 != monitor_wait(&queue->not_empty_monitor)) {
            return NULL; // Wait failed
        }
        
        //printf("DEBUG: monitor_wait returned, retrying\n");
        // Condition changed - retry the operation
    }
}


void consumer_producer_signal_finished(consumer_producer_t* queue) {
    if (NULL == queue) {

        return;
    }
    monitor_signal(&queue->finished_monitor);
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    if (NULL == queue) {
        return -1;
    }

    if (0 != monitor_wait(&queue->finished_monitor)) {
        return -1;
    }
    return 0;
}

/*** HELPER FUNCTIONS ***/

//called if initialization fails at any stage - we will clean up resources allocated so far
// depending on the stage of initialization, we will clean up different resources
// stage 0: items array allocated, stage 1: items array + queue_mutex initialized, etc.
static void cleanup_partial_init(consumer_producer_t* queue, int stage) {
    if (NULL == queue) {
        return;
    }

    if (stage >= 3) {
        monitor_destroy(&queue->not_empty_monitor);
    }
    if (stage >= 2) {
        monitor_destroy(&queue->not_full_monitor);
    }
    if (stage >= 1 && queue->mutex_initialized) {
        pthread_mutex_destroy(&queue->queue_mutex);
        queue->mutex_initialized = 0;
    }
    if (NULL != queue->items) {
        free(queue->items);
        queue->items = NULL;
    }
}
