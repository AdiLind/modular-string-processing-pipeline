#include "monitor.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

int monitor_init(monitor_t* monitor) {
    int is_init_success;
    if (monitor == NULL) {
        return -1;
    }

    monitor->signaled = 0;
    //monitor->count = 0;
    is_init_success = pthread_mutex_init(&monitor->mutex, NULL);
    if (is_init_success != 0) {
        return -1;
    }

    is_init_success = pthread_cond_init(&monitor->condition, NULL);
    if (is_init_success != 0) {
        //init failed, clean up mutex
        pthread_mutex_destroy(&monitor->mutex);
        return -1;
    }

    return 0;
}

/*
* get the monitor pointer and release the monitor sorcse 
* if there is error in the process of destroying the monitor
* it will log a warning message and continue to free the other resources
*/
void monitor_destroy(monitor_t* monitor) {
    if (monitor == NULL) {
        return;
    }
    int is_destroyed;

    is_destroyed = pthread_mutex_destroy(&monitor->mutex);
    if(is_destroyed != 0) {
        //Log error maybe mutex still locked
        fprintf(stderr, "[monitor_destroy] Warning: pthread_mutex_destroy failed with error %d\n", is_destroyed);
    }

    is_destroyed = pthread_cond_destroy(&monitor->condition);
    if(is_destroyed != 0) {
        
        fprintf(stderr, "[monitor_destroy] Warning: pthread_cond_destroy failed with error %d\n", is_destroyed);
    }
    monitor->signaled = 0;
    //monitor->count = 0;  
}

void monitor_signal(monitor_t* monitor) 
{
    int is_locked;
    int is_unlocked;
    if (NULL == monitor) {
        return;
    }
    // Lock the mutex before signaling
    is_locked = pthread_mutex_lock(&monitor->mutex);

    if (is_locked != 0) {
        fprintf(stderr, "[monitor_signal] Error: pthread_mutex_lock failed with error %d\n", is_locked);
        return;
    }

    monitor->signaled = 1;
    //monitor->count++;
    //pthread_cond_signal(&monitor->condition);
    pthread_cond_broadcast(&monitor->condition); //wake all waiting threads

    is_unlocked = pthread_mutex_unlock(&monitor->mutex);
    if (is_unlocked != 0) {
        fprintf(stderr, "[monitor_signal] Error: pthread_mutex_unlock failed with error %d\n", is_unlocked);
    }
}

void monitor_reset(monitor_t* monitor) 
{
    int is_locked;
    int is_unlocked;
    if (NULL == monitor) {
        return;
    }

    is_locked = pthread_mutex_lock(&monitor->mutex);
    if (is_locked != 0) {
        fprintf(stderr, "[monitor_reset] Error: pthread_mutex_lock failed with error %d\n", is_locked);
        return;
    }

    monitor->signaled = 0;
    //monitor->count = 0; 

    is_unlocked = pthread_mutex_unlock(&monitor->mutex);
    if (is_unlocked != 0) {
        fprintf(stderr, "[monitor_reset] Error: pthread_mutex_unlock failed with error %d\n", is_unlocked);
    }
}

int monitor_wait(monitor_t* monitor) 
{
    int lock_result = 0;
    int unlock_result = 0;
    int wait_result  = 0;

    if (NULL == monitor) {
        return -1;
    }

    lock_result  = pthread_mutex_lock(&monitor->mutex);
    if (lock_result  != 0) {
        fprintf(stderr, "[monitor_wait] Error: pthread_mutex_lock failed with error %d\n", lock_result );
        return -1;
    }

    while (0 == monitor->signaled)
    {
        //wait for the condition variable, we releases the mutex while waiting
        wait_result = pthread_cond_wait(&monitor->condition, &monitor->mutex);
        if (wait_result != 0) {
            fprintf(stderr, "[monitor_wait] Error: pthread_cond_wait failed with error %d\n", wait_result);
           pthread_mutex_unlock(&monitor->mutex); // this line maybe not correct what if the mutex is already locked? or undefined?
            return -1;
        }
    }
    
    //after consuming the signal we need to reset signaled

    // monitor->signaled = 0; 

    //monitor->count--; // Decrement the count of pending signals

    unlock_result  = pthread_mutex_unlock(&monitor->mutex);
    if (unlock_result != 0) {
        fprintf(stderr, "[monitor_wait] Error: pthread_mutex_unlock failed with error %d\n", unlock_result);
        return -1;
    }

    return 0;
}
