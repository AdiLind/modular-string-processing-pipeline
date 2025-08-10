#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

    // initialize the queue
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->items = NULL;
    queue->mutex_initialized = 0; 

    // Allocate items array
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

    if(queue->mutex_initialized) //lock the mutex before destroying
    {
        pthread_mutex_lock(&queue->queue_mutex);
    }

    // free the items in the queue
    if (NULL != queue->items) 
    {
        //int i;
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

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    // TODO: Implement
    return NULL;
}

char* consumer_producer_get(consumer_producer_t* queue) {
    // TODO: Implement
    return NULL;
}

void consumer_producer_signal_finished(consumer_producer_t* queue) {
    // TODO: Implement
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    // TODO: Implement
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
