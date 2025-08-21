#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


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
        
        // Check condition while holding lock
        if (queue->count < queue->capacity) {
            // Space available - perform operation
            char* copy_of_item = strdup(item);
            if (NULL == copy_of_item) {
                pthread_mutex_unlock(&queue->queue_mutex);
                return "Failed to copy item string";
            }
            
            // Add item
            queue->items[queue->tail] = copy_of_item;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;
            
            // Release lock before signaling
            pthread_mutex_unlock(&queue->queue_mutex);
            
            // Signal that queue is not empty (wake up consumers)
            monitor_signal(&queue->not_empty_monitor);
            
            return NULL; // Success
        }
        
        // Condition not met - prepare to wait
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Wait for condition to change
        monitor_reset(&queue->not_full_monitor);
        if (0 != monitor_wait(&queue->not_full_monitor)) {
            return "Failed to wait for not_full condition";
        }
        
        // Condition changed - retry the operation
    }
}



/* 
**old version 
const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {

    if (NULL == queue || NULL == item) {
        return "Queue or item pointer is NULL";
    }

    if(NULL == queue->items) {
        return "Queue items array is not initialized";
    }
    
    // if (queue->count >= queue->capacity) {
    //     return "Queue is full";
    // }

    //wait until queue is full
    pthread_mutex_lock(&queue->queue_mutex);
    while (queue->count >= queue->capacity) 
    {
        // queue is full now so we make manual reset not_full monitor before waiting
        pthread_mutex_unlock(&queue->queue_mutex);
        monitor_reset(&queue->not_full_monitor);
        
        
        //block until space to becomes available
        if (0 != monitor_wait(&queue->not_full_monitor)) {
            return "Failed to wait for not_full condition";
        }
        
        //reacquire mutex before checking condition again
        pthread_mutex_lock(&queue->queue_mutex);
    }

    // now we can safely add the item (we have the mutex locked and queue is not full)
    //make a copy of the string
    char* copy_of_item = strdup(item);
    if (NULL == copy_of_item) {
        pthread_mutex_unlock(&queue->queue_mutex);
        return "Failed to copy item string";
    }
    
    // add the item to the queue
    queue->items[queue->tail] = copy_of_item;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    //now we signal that queue is not empty
    monitor_signal(&queue->not_empty_monitor);

    //if the queue is not full after adding the item, we signal not_full_monitor
    if (queue->count < queue->capacity) {
        monitor_signal(&queue->not_full_monitor);
    }

    pthread_mutex_unlock(&queue->queue_mutex);
    return NULL; //success 
}

*/

char* consumer_producer_get(consumer_producer_t* queue) {
    if (NULL == queue) {
        //printf("DEBUG: queue is NULL\n");
        return NULL;
    }
    
    if (NULL == queue->items) {
        //printf("DEBUG: queue->items is NULL\n");
        return NULL;
    }

    
    //printf("DEBUG: About to lock mutex for get operation\n");
    //printf("DEBUG: queue->count = %d, queue->head = %d, queue->capacity = %d\n", queue->count, queue->head, queue->capacity);
    while (1) {
        pthread_mutex_lock(&queue->queue_mutex);

        //printf("DEBUG: Locked mutex, checking condition (count > 0)\n");
        //printf("DEBUG: queue->count = %d\n", queue->count);
        
        // Check condition while holding lock
        if (queue->count > 0) {

            //printf("DEBUG: Found items, getting item at head = %d\n", queue->head);
            //printf("DEBUG: About to access queue->items[%d]\n", queue->head);
            //printf("DEBUG: queue->items pointer = %p\n", (void*)queue->items);  
            //printf("DEBUG: queue->items[%d] pointer check\n", queue->head); 
            // Items available - perform operation
            char* item = queue->items[queue->head];
            //printf("DEBUG: Successfully accessed item: %s\n", item ? item : "NULL");
            queue->items[queue->head] = NULL;
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;

            //printf("DEBUG: Updated queue state - count: %d, head: %d\n", queue->count, queue->head);
            
            // Release lock before signaling
            pthread_mutex_unlock(&queue->queue_mutex);
            
            // Signal that queue is not full (wake up producers)
            monitor_signal(&queue->not_full_monitor);
            
            return item; // Success
        }
        
        //printf("DEBUG: Queue is empty, preparing to wait\n");
        // Condition not met - prepare to wait
        pthread_mutex_unlock(&queue->queue_mutex);
        
        // Wait for condition to change
        monitor_reset(&queue->not_empty_monitor);
        //printf("DEBUG: About to wait on not_empty_monitor\n");
        if (0 != monitor_wait(&queue->not_empty_monitor)) {
            //printf("DEBUG: monitor_wait failed\n");
            return NULL; // Wait failed
        }
        
        //printf("DEBUG: monitor_wait returned, retrying\n");
        // Condition changed - retry the operation
    }
}


/*
* old version
char* consumer_producer_get(consumer_producer_t* queue) 
{
    if (NULL == queue) {
        return NULL;
    }
    if (NULL == queue->items) {
        return NULL; // items array is not initialized
    }

    // wait until queue is not empty
    pthread_mutex_lock(&queue->queue_mutex);
    while (queue->count <= 0) {
        // queue is empty now so we make manual reset not_empty monitor before waiting
        pthread_mutex_unlock(&queue->queue_mutex);
        monitor_reset(&queue->not_empty_monitor);
        
        //wait for an item to become available 
        if (0 != monitor_wait(&queue->not_empty_monitor)) {
            return NULL;
        }
        
        //reacquire mutex and check condition again
        pthread_mutex_lock(&queue->queue_mutex);
    }

    // now we can safely get the item (we have the mutex locked and queue is not empty)
    // get the item from the queue
    char* item = queue->items[queue->head];
    queue->items[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    monitor_signal(&queue->not_full_monitor);
    // if the queue is not empty after getting the item, we signal not_empty_monitor
    if (queue->count > 0) {
        monitor_signal(&queue->not_empty_monitor);
    }

    pthread_mutex_unlock(&queue->queue_mutex);
    return item; //return the item (the caller responsible for freeing it - we made a copy) #TODO: check if this is the correct approach

}

*/

void consumer_producer_signal_finished(consumer_producer_t* queue) {
    //printf("DEBUG: signal_finished called\n");
    if (NULL == queue) {
        //printf("DEBUG: queue is NULL!\n");
        return;
    }
    //printf("DEBUG: calling monitor_signal\n");
    monitor_signal(&queue->finished_monitor);
    //printf("DEBUG: monitor_signal completed\n");
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    //printf("DEBUG: wait_finished called\n");
    if (NULL == queue) {
        //printf("DEBUG: queue is NULL!\n");
        return -1;
    }
    //printf("DEBUG: calling monitor_wait\n");
    if (0 != monitor_wait(&queue->finished_monitor)) {
        //printf("DEBUG: monitor_wait failed\n");
        return -1;
    }
    //printf("DEBUG: monitor_wait succeeded\n");
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
