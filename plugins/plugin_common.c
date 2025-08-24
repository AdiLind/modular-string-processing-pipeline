#define _GNU_SOURCE
#include "plugin_common.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple single context per .so instance - this is the correct approach
plugin_context_t g_plugin_context = {0};

// Logging Functions
void log_info(plugin_context_t* plugin_context, const char* message)
{
    if (NULL == plugin_context || NULL == message) {
        return;
    }
    
    fprintf(stdout, "[INFO][%s] - %s\n", plugin_context->name ? plugin_context->name : "Unknown", message);
}

void log_error(plugin_context_t* plugin_context, const char* error_message)
{
    if (NULL == plugin_context || NULL == error_message) 
    {
        return;
    }
    
    fprintf(stderr, "[ERROR][%s] - %s\n", plugin_context->name ? plugin_context->name : "Unknown", error_message);
}

void* plugin_consumer_thread(void* arg)
{
    plugin_context_t* plugin_context = (plugin_context_t*)arg;
    if (NULL == plugin_context) {
        return NULL;
    }

    // Signal thread ready
    pthread_mutex_lock(&plugin_context->ready_mutex);
    plugin_context->thread_ready = 1;
    pthread_cond_signal(&plugin_context->ready_cond);
    pthread_mutex_unlock(&plugin_context->ready_mutex);

    while(!plugin_context->finished) {
        char* input_string = consumer_producer_get(plugin_context->queue);

        if (NULL == input_string) {
            if (plugin_context->finished) break;
            continue;
        }

        if (plugin_context->finished) {
            free(input_string); 
            break;
        }

        if (0 == strcmp(input_string, "<END>")) {
            // Forward END signal
            if (plugin_context->next_place_work) {
                plugin_context->next_place_work(input_string);
            }
            
            plugin_context->finished = 1;
            consumer_producer_signal_finished(plugin_context->queue);
            free(input_string);
            break;
        }

        // Process the input
        const char* processed = plugin_context->process_function(input_string);
        if (processed) {
            // Forward to next plugin
            if (plugin_context->next_place_work) {
                plugin_context->next_place_work(processed);
            }
            
            if (processed != input_string) {
                free((char*)processed);
            }
        }
        free(input_string);
    }
    
    return NULL;
}

const char* common_plugin_init(const char* (*process_function)(const char*), 
                              const char* name, int queue_size) {
    
    // Clear context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    // Initialize synchronization
    if (pthread_mutex_init(&g_plugin_context.ready_mutex, NULL) != 0) {
        return "Failed to initialize mutex";
    }
    
    if (pthread_cond_init(&g_plugin_context.ready_cond, NULL) != 0) {
        pthread_mutex_destroy(&g_plugin_context.ready_mutex);
        return "Failed to initialize condition";
    }

    // Set up context
    g_plugin_context.name = name;
    g_plugin_context.process_function = process_function;
    
    // Initialize queue
    g_plugin_context.queue = (consumer_producer_t*)malloc(sizeof(consumer_producer_t));
    if (!g_plugin_context.queue) {
        pthread_cond_destroy(&g_plugin_context.ready_cond);
        pthread_mutex_destroy(&g_plugin_context.ready_mutex);
        return "Failed to allocate queue";
    }

    memset(g_plugin_context.queue, 0, sizeof(consumer_producer_t));
    const char* error = consumer_producer_init(g_plugin_context.queue, queue_size);
    if (error) {
        free(g_plugin_context.queue);
        pthread_cond_destroy(&g_plugin_context.ready_cond);
        pthread_mutex_destroy(&g_plugin_context.ready_mutex);
        return error;
    }
    
    // Create consumer thread
    if (pthread_create(&g_plugin_context.consumer_thread, NULL, 
                       plugin_consumer_thread, &g_plugin_context) != 0) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
        pthread_cond_destroy(&g_plugin_context.ready_cond);
        pthread_mutex_destroy(&g_plugin_context.ready_mutex);
        return "Failed to create thread";
    }

    g_plugin_context.thread_created = 1;

    // Wait for thread ready
    pthread_mutex_lock(&g_plugin_context.ready_mutex);
    while (!g_plugin_context.thread_ready) {
        pthread_cond_wait(&g_plugin_context.ready_cond, &g_plugin_context.ready_mutex);
    }
    pthread_mutex_unlock(&g_plugin_context.ready_mutex);

    g_plugin_context.initialized = 1;
    return NULL;
}

PLUGIN_EXPORT
const char* plugin_place_work(const char* str) {
    if (!g_plugin_context.initialized || !str) {
        return "Plugin not ready";
    }
    return consumer_producer_put(g_plugin_context.queue, str);
}

PLUGIN_EXPORT
void plugin_attach(const char* (*next_place_work)(const char*)) {
    g_plugin_context.next_place_work = next_place_work;
}

PLUGIN_EXPORT
const char* plugin_wait_finished(void) {
    if (!g_plugin_context.initialized || !g_plugin_context.queue) {
        return "Plugin not ready";
    }
    return (consumer_producer_wait_finished(g_plugin_context.queue) == 0) ? NULL : "Wait failed";
}

PLUGIN_EXPORT
const char* plugin_fini(void) {
    if (!g_plugin_context.initialized) {
        return NULL;
    }

    g_plugin_context.finished = 1;

    if (g_plugin_context.queue) {
        monitor_signal(&g_plugin_context.queue->not_empty_monitor);
    }

    if (g_plugin_context.thread_created) {
        pthread_join(g_plugin_context.consumer_thread, NULL);
    }

    if (g_plugin_context.queue) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
    }
    
    pthread_cond_destroy(&g_plugin_context.ready_cond);
    pthread_mutex_destroy(&g_plugin_context.ready_mutex);
    
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    return NULL;
}

PLUGIN_EXPORT
const char* plugin_get_name(void) {
    return g_plugin_context.name ? g_plugin_context.name : "Unknown Plugin";
}