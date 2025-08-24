#define _GNU_SOURCE
#include "plugin_common.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//create a global plugin context because each plugin has its own instance
// plugin_context_t g_plugin_context = { .name = NULL, .queue = NULL, .consumer_thread = 0,
//                                       .next_place_work = NULL, .process_function = NULL, 
//                                       .initialized = 0, .finished = 0, .thread_created = 0};

// support multiple instances of same plugin type - dynamic allocation based on CLI args
static plugin_context_t** plugin_instances_array = NULL;
static int total_plugin_instances = 0;
static int max_allowed_plugin_instances = 0;
static pthread_mutex_t plugin_instances_mutex = PTHREAD_MUTEX_INITIALIZER;

// track which context to use for each operation
static int current_init_instance_index = 0;
static int next_place_work_instance_index = 0;
static int next_wait_instance_index = 0;
static int next_fini_instance_index = 0;

/***** Helper Function Declaration  ******/  
//static void cleanup_plugin_resources(void);
//static void free_if_allocated(const char* processed, const char* original);
static void forward_to_next_plugin(plugin_context_t* ctx, const char* str);
static const char* validate_init_params(const char* (*process_func)(const char*), 
                                                      const char* name, int queue_size);
static const char* ensure_plugin_instances_array_allocated(int max_instances);



/* /////////////////////  */
//  Logging Functions
/* /////////////////////  */
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
        //TODO: should we log an error here while trying to log an error?
        //fprintf(stderr, "[ERROR][Unknown] - Error logging failed: plugin_context or error_message is NULL\n");
        return;
    }
    
    fprintf(stderr, "[ERROR][%s] - %s\n", plugin_context->name ? plugin_context->name : "Unknown", error_message);
}

// this function run in a separate thread and processes items from the queue
// it retrieves strings from the queue, processes them, and forwards them to the next plugin if attached
void* plugin_consumer_thread(void* arg)
{
    plugin_context_t* plugin_context = (plugin_context_t*)arg;
    if (NULL == plugin_context)
    {
        log_error(plugin_context, "Consumer thread received NULL context");
        return NULL;
    }

    if (NULL == plugin_context->queue || NULL == plugin_context->process_function)
    {
        log_error(plugin_context, "The consumer thread get invalid context");
        return NULL;
    }

    //signal that the thread is ready
    // this is used to notify the main thread that the consumer thread is ready to process items
    pthread_mutex_lock(&plugin_context->ready_mutex);
    plugin_context->thread_ready = 1;
    pthread_cond_signal(&plugin_context->ready_cond);
    pthread_mutex_unlock(&plugin_context->ready_mutex);

    while(!plugin_context->finished)
    {
        // retrieve next item from queue, blocks if empty
        char* input_string = consumer_producer_get(plugin_context->queue);
        // if(NULL == input_string)
        // {
        //     log_error(plugin_context, "Failed to retrieve string from queue");
        //     continue;
        // }

        if (NULL == input_string) {
            // Queue returned NULL - check if we're shutting down
            if (plugin_context->finished) {
                break;
            }
            continue; // Spurious wakeup, try again
        }

        // Check if we should exit (finished flag set during shutdown)
        if (plugin_context->finished) {
            free(input_string); 
            break;
        }

        // //check if we received a shutdown signal
        // if( 0 == strcmp(input_string, "<END>"))
        // {
        //     //if we get <END> through this plugin's transformation 
        //     const char* processed_signal = plugin_context->process_function(input_string); 

        //     // forward the signal to the next plugin if attached
        //     forward_to_next_plugin(plugin_context, processed_signal ? processed_signal : input_string); 

        //     //free the allocated memory only if transformation allocated new memory
        //     if (processed_signal != NULL && processed_signal != input_string) {
        //         free((char*)processed_signal);
        //     }
        //     free(input_string);  // we always need to free the original input

        //     plugin_context->finished = 1;
        //     consumer_producer_signal_finished(plugin_context->queue);
        //     break;
        // }

        if (0 == strcmp(input_string, "<END>")) {
            
            //fprintf(stderr, "[DEBUG] %s: Processing END signal\n", plugin_context->name);

            // Forward END signal without transformation
            forward_to_next_plugin(plugin_context, input_string);
            
            // Mark as finished BEFORE signaling
            plugin_context->finished = 1;
            
            // Signal that we're finished
            consumer_producer_signal_finished(plugin_context->queue);
            
            // Clean up and exit
            free(input_string);
            break;
        }


        // if got into this  line so the input string is not a <END>, so we need to process it
        const char* processed = plugin_context->process_function(input_string);
        if (NULL == processed) {
            log_error(plugin_context, "Processing function returned NULL");
            free(input_string);
            continue;
        }

        //forward to next plugin if exists
        forward_to_next_plugin(plugin_context, processed);

        //clean the mass before we exit
        // free the processed string if it was allocated by the process_function
        if (processed != input_string) {
            free((char*)processed); 

        }
        free(input_string);
    }
    
    return NULL;
}

/// עדי של העתיד שלום, אתה עכשיו צריך לממש את ההחלק של הINIT ////
// בהצלחה גיבור

const char* common_plugin_init(const char* (*process_function)(const char*), 
                              const char* name, int queue_size) {

    const char* error;
    const char* validation_error = validate_init_params(process_function, name, queue_size);
    if (NULL != validation_error) {
        return validation_error;
    }

    // OLD CODE - single context approach
    // memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    // 
    // //TODO: testing the neccessary of those validations - should move to the validate_init_params function
    // if (pthread_mutex_init(&g_plugin_context.ready_mutex, NULL) != 0) {
    //     return "Failed to initialize ready mutex";
    // }
    
    // NEW CODE - multiple context support with dynamic allocation
    pthread_mutex_lock(&plugin_instances_mutex);
    
    // first time initialization - allocate array based on CLI args count
    if (NULL == plugin_instances_array) {
        // we need to get max instances from somewhere - for now use a reasonable default
        // this will be set by main() or we can get it from environment/config
        max_allowed_plugin_instances = 50; // TODO: make this configurable from CLI
        error = ensure_plugin_instances_array_allocated(max_allowed_plugin_instances);
        if (NULL != error) {
            pthread_mutex_unlock(&plugin_instances_mutex);
            return error;
        }
    }
    
    if (total_plugin_instances >= max_allowed_plugin_instances) {
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Maximum plugin instances reached";
    }
    
    // allocate new context for this instance
    plugin_context_t* current_plugin_context = (plugin_context_t*)calloc(1, sizeof(plugin_context_t));
    if (NULL == current_plugin_context) {
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Failed to allocate context";
    }
    
    //TODO: testing the neccessary of those validations - should move to the validate_init_params function
    if (pthread_mutex_init(&current_plugin_context->ready_mutex, NULL) != 0) {
        free(current_plugin_context);
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Failed to initialize ready mutex";
    }
    
    if (pthread_cond_init(&current_plugin_context->ready_cond, NULL) != 0) {
        pthread_mutex_destroy(&current_plugin_context->ready_mutex);
        free(current_plugin_context);
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Failed to initialize ready condition";
    }

    //init context
    current_plugin_context->thread_ready = 0;
    current_plugin_context->name = name;
    current_plugin_context->process_function = process_function;
    current_plugin_context->next_place_work = NULL;
    current_plugin_context->finished = 0;
    current_plugin_context->thread_created = 0;
    current_plugin_context->initialized = 0;


    //init and allocate queue
    current_plugin_context->queue = (consumer_producer_t*)malloc(sizeof(consumer_producer_t));
    if (NULL == current_plugin_context->queue) {
        pthread_mutex_destroy(&current_plugin_context->ready_mutex);
        pthread_cond_destroy(&current_plugin_context->ready_cond);
        free(current_plugin_context);
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Failed to allocate memory for queue structure";
    }

    memset(current_plugin_context->queue, 0, sizeof(consumer_producer_t));
    error = consumer_producer_init(current_plugin_context->queue, queue_size);
    if (NULL != error) {
        free(current_plugin_context->queue);
        pthread_mutex_destroy(&current_plugin_context->ready_mutex);
        pthread_cond_destroy(&current_plugin_context->ready_cond);
        free(current_plugin_context);
        pthread_mutex_unlock(&plugin_instances_mutex);
        return error;
    }
    
    //create the consumer thread
    if(0 != pthread_create(&current_plugin_context->consumer_thread, NULL, plugin_consumer_thread, current_plugin_context))
    {
        consumer_producer_destroy(current_plugin_context->queue);
        free(current_plugin_context->queue);
        pthread_mutex_destroy(&current_plugin_context->ready_mutex);
        pthread_cond_destroy(&current_plugin_context->ready_cond);
        free(current_plugin_context);
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "Failed occour when creating consumer thread";
    }

    current_plugin_context->thread_created = 1;

    // waiting for the thread to be ready
    pthread_mutex_lock(&current_plugin_context->ready_mutex);

    while (!current_plugin_context->thread_ready)
    {
        pthread_cond_wait(&current_plugin_context->ready_cond, &current_plugin_context->ready_mutex);
    }

    pthread_mutex_unlock(&current_plugin_context->ready_mutex);

    current_plugin_context->initialized = 1;
    
    // store context and update index
    plugin_instances_array[total_plugin_instances] = current_plugin_context;
    current_init_instance_index = total_plugin_instances;
    total_plugin_instances++;
    
    pthread_mutex_unlock(&plugin_instances_mutex);

    return NULL;
}

// each plugin should implement this function
// const char* plugin_init(int queue_size) {
//     // TODO: Implement
//     return NULL;
// }


//old version of plugin_fini

// const char* plugin_fini(void) 
// {

//     if (!g_plugin_context.initialized){ return "Plugin not initialized"; }

//     // we mark it as finished to stop the consumer thread
//     g_plugin_context.finished = 1;
//     if (g_plugin_context.thread_created && !g_plugin_context.finished) 
//     {
//         // Send <END> to wake up the thread and make it exit
//         const char* error = plugin_place_work("<END>");
//         if (error != NULL) {
//             log_error(&g_plugin_context, "Failed to send termination signal");
//         }
        
//         // Wait for the thread to process the END signal
//         plugin_wait_finished();
//     }

//     if (g_plugin_context.thread_created) 
//     {
//         if( 0 != pthread_join(g_plugin_context.consumer_thread, NULL))
//         {
//             log_error(&g_plugin_context, "Failed to join consumer thread during finalization");
//         }
//         g_plugin_context.thread_created = 0;

//     }

//     // destroy and free queue cleanup all the resources
//     if (NULL != g_plugin_context.queue) 
//     {
//         consumer_producer_destroy(g_plugin_context.queue);
//         free(g_plugin_context.queue);
//         g_plugin_context.queue = NULL;
//     }
    
//     // reset the context
//     g_plugin_context.initialized = 0;
//     g_plugin_context.finished = 0;
//     g_plugin_context.name = NULL;
//     g_plugin_context.process_function = NULL;
//     g_plugin_context.next_place_work = NULL;
//     pthread_mutex_destroy(&g_plugin_context.ready_mutex);
//     pthread_cond_destroy(&g_plugin_context.ready_cond);

//     return NULL; 

// }

const char* plugin_fini(void) 
{
    // OLD CODE - single context
    // if (!g_plugin_context.initialized) { 
    //     return "Plugin not initialized"; 
    // }

    // NEW CODE - multiple contexts support
    pthread_mutex_lock(&plugin_instances_mutex);
    
    if (next_fini_instance_index >= total_plugin_instances) {
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "No more contexts to finalize";
    }
    
    plugin_context_t* current_plugin_context = plugin_instances_array[next_fini_instance_index];
    plugin_instances_array[next_fini_instance_index] = NULL;  // clear the reference
    next_fini_instance_index++;
    
    pthread_mutex_unlock(&plugin_instances_mutex);
    
    if (!current_plugin_context->initialized) {
        free(current_plugin_context);  // clean up uninitialized context
        return NULL;  // not an error, just nothing to do
    }

    // Mark as finished to stop the consumer thread
    current_plugin_context->finished = 1;

    //wakeup all the waiting threads to process shutdown
    if(NULL != current_plugin_context->queue) 
    {
        monitor_signal(&current_plugin_context->queue->not_empty_monitor);
        monitor_signal(&current_plugin_context->queue->not_full_monitor);
    }

    //wait for the consumer thread to finish
    if (current_plugin_context->thread_created) 
    {
        int join_result = pthread_join(current_plugin_context->consumer_thread, NULL);
        if( 0 != join_result)
        {
            pthread_cancel(current_plugin_context->consumer_thread);
            pthread_join(current_plugin_context->consumer_thread, NULL);
            log_error(current_plugin_context, "Failed to join consumer thread during finalization");
        }
        current_plugin_context->thread_created = 0;
    }

    // Clean up queue and its remaining contents
    if (NULL != current_plugin_context->queue) {
        consumer_producer_destroy(current_plugin_context->queue);
        free(current_plugin_context->queue);
        current_plugin_context->queue = NULL;
    }
    
    // Clean up synchronization primitives
    pthread_mutex_destroy(&current_plugin_context->ready_mutex);
    pthread_cond_destroy(&current_plugin_context->ready_cond);
    
    // free the context itself
    free(current_plugin_context);
    
    // reset indices when all contexts are finalized
    pthread_mutex_lock(&plugin_instances_mutex);
    if (next_fini_instance_index >= total_plugin_instances) {
        // cleanup the instances array itself
        if (NULL != plugin_instances_array) {
            free(plugin_instances_array);
            plugin_instances_array = NULL;
        }
        total_plugin_instances = 0;
        max_allowed_plugin_instances = 0;
        current_init_instance_index = 0;
        next_place_work_instance_index = 0;
        next_wait_instance_index = 0;
        next_fini_instance_index = 0;
    }
    pthread_mutex_unlock(&plugin_instances_mutex);

    return NULL;
}

PLUGIN_EXPORT
const char* plugin_place_work(const char* str) 
{
    // OLD CODE - single context
    // if (!g_plugin_context.initialized) { return "Plugin not initialized"; }
    // if (NULL == str) { return "Input string is NULL"; }
    // const char* error = consumer_producer_put(g_plugin_context.queue, str);
    // if (NULL != error) {
    //     return error;
    // }
    // return NULL;
    
    // NEW CODE - multiple contexts support
    if (NULL == str) { return "Input string is NULL"; }
    
    pthread_mutex_lock(&plugin_instances_mutex);
    
    if (total_plugin_instances == 0) {
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "No plugin instances initialized";
    }
    
    // For pipeline plugins, each .so should only handle one instance at a time
    // Use the most recent instance (the one that was attached)
    plugin_context_t* current_plugin_context = plugin_instances_array[total_plugin_instances - 1];
    
    pthread_mutex_unlock(&plugin_instances_mutex);
    
    if (!current_plugin_context->initialized) {
        return "Plugin context not initialized";
    }
    
    const char* error = consumer_producer_put(current_plugin_context->queue, str);
    if (NULL != error) {
        return error;
    }

    return NULL; 
}

PLUGIN_EXPORT
const char* plugin_get_name(void) {
    // OLD CODE - single context
    // return g_plugin_context.name ? g_plugin_context.name : "Unknown Plugin";
    
    // NEW CODE - multiple contexts support
    pthread_mutex_lock(&plugin_instances_mutex);
    
    if (total_plugin_instances > 0 && plugin_instances_array[0] != NULL) {
        const char* plugin_name = plugin_instances_array[0]->name;
        pthread_mutex_unlock(&plugin_instances_mutex);
        return plugin_name ? plugin_name : "Unknown Plugin";
    }
    
    pthread_mutex_unlock(&plugin_instances_mutex);
    return "Unknown Plugin";
}


PLUGIN_EXPORT
void plugin_attach(const char* (*next_place_work)(const char*))
{
    // OLD CODE - single context
    // g_plugin_context.next_place_work = next_place_work;
    
    // NEW CODE - multiple contexts support
    pthread_mutex_lock(&plugin_instances_mutex);
    if (current_init_instance_index < total_plugin_instances) {
        plugin_instances_array[current_init_instance_index]->next_place_work = next_place_work;
    }
    pthread_mutex_unlock(&plugin_instances_mutex);
}

PLUGIN_EXPORT
const char* plugin_wait_finished(void) 
{
    // OLD CODE - single context
    // if(!g_plugin_context.initialized) { return "Plugin not initialized"; }
    // if(NULL == g_plugin_context.queue) { return "Queue not initialized"; }
    // fprintf(stderr, "[DEBUG] %s: Waiting for finished signal\n", g_plugin_context.name);
    // //wait until we grt the finish signal from the queue
    // int wait_result = consumer_producer_wait_finished(g_plugin_context.queue);
    // if (0 != wait_result) {
    //     return "fail while waiting for finished condition";
    // }
    // return NULL;
    
    // NEW CODE - multiple contexts support
    pthread_mutex_lock(&plugin_instances_mutex);
    
    if (next_wait_instance_index >= total_plugin_instances) {
        pthread_mutex_unlock(&plugin_instances_mutex);
        return "No more contexts to wait for";
    }
    
    plugin_context_t* current_plugin_context = plugin_instances_array[next_wait_instance_index++];
    pthread_mutex_unlock(&plugin_instances_mutex);
    
    if (!current_plugin_context->initialized) {
        return "Plugin context not initialized";
    }
    
    if (NULL == current_plugin_context->queue) {
        return "Queue not initialized";
    }
    
    fprintf(stderr, "[DEBUG] %s (instance %d): Waiting for finished signal\n", 
            current_plugin_context->name, next_wait_instance_index - 1);
    
    //wait until we grt the finish signal from the queue
    int wait_result = consumer_producer_wait_finished(current_plugin_context->queue);
    if (0 != wait_result) {
        return "fail while waiting for finished condition";
    }

    return NULL;
}

/* ////////////////////////////////////////*/
///// Helper Function Implementations ////
/* ///////////////////////////////////////////*/
static const char* validate_init_params(const char* (*process_func)(const char*), 
                                        const char* name, int queue_size) {
    if (NULL == process_func) {
        return "Process function is NULL";
    }
    
    if (NULL == name) {
        return "Plugin name is NULL";
    }
    
    if (queue_size <= 0) {
        return "Invalid queue size cannot be zero or negative";
    }
    
    // if (g_plugin_context.initialized) {
    //     return "Plugin already initialized";
    // }
    
    return NULL; 
}

// static void free_if_allocated(const char* processed, const char* original) {
//     if (processed != NULL && processed != original) {
//         free((char*)processed);
//     }
// }

static void forward_to_next_plugin(plugin_context_t* curr_plugin, const char* str) {
    if (NULL != curr_plugin->next_place_work) {
        const char* error_handler = curr_plugin->next_place_work(str);
        if (NULL != error_handler) {
            log_error(curr_plugin, error_handler);
        }
    }
}

static const char* ensure_plugin_instances_array_allocated(int max_instances) {
    if (NULL != plugin_instances_array) {
        return NULL; // already allocated
    }
    
    plugin_instances_array = (plugin_context_t**)calloc(max_instances, sizeof(plugin_context_t*));
    if (NULL == plugin_instances_array) {
        return "Failed to allocate plugin instances array";
    }
    
    return NULL;
}