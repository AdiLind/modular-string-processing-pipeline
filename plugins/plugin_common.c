#include "plugin_common.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//create a global plugin context because each plugin has its own instance
plugin_context_t g_plugin_context = { .name = NULL, .queue = NULL, .consumer_thread = 0,
                                      .next_place_work = NULL, .process_function = NULL, 
                                      .initialized = 0, .finished = 0, .thread_created = 0};

/***** Helper Function Declaration  ******/  
static void cleanup_plugin_resources(void);
static void free_if_allocated(const char* processed, const char* original);
static void forward_to_next_plugin(plugin_context_t* ctx, const char* str);
static const char* validate_init_params(const char* (*process_func)(const char*), 
                                                      const char* name, int queue_size);



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

    while(!plugin_context->finished)
    {
        // Retrieve next item from queue, blocks if empty
        char* input_string = consumer_producer_get(plugin_context->queue);
        if(NULL == input_string)
        {
            log_error(plugin_context, "Failed to retrieve string from queue");
            continue;
        }

        //check if we received a shutdown signal
        if( 0 == strcmp(input_string, "<END>"))
        {
            //if we get <END> through this plugin's transformation 
            const char* processed_signal = plugin_context->process_function(input_string); 
            // forward the signal to the next plugin if attached
            forward_to_next_plugin(plugin_context, processed_signal ? processed_signal : input_string); 
            //free the allocated memory
            free_if_allocated(processed_signal, input_string);
            free(input_string);

            plugin_context->finished = 1;
            consumer_producer_signal_finished(plugin_context->queue);
            //break;
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
        free_if_allocated(processed, input_string);
        free(input_string);
    }
    
    return NULL;
}



const char* common_plugin_init(const char* (*process_function)(const char*), 
                              const char* name, int queue_size) {
    // TODO: Implement
    return NULL;
}

const char* plugin_init(int queue_size) {
    // TODO: Implement
    return NULL;
}

const char* plugin_fini(void) {
    // TODO: Implement
    return NULL;
}

const char* plugin_place_work(const char* str) {
    // TODO: Implement
    return NULL;
}

const char* plugin_get_name(void) {
    // TODO: Implement
    return NULL;
}

void plugin_attach(const char* (*next_place_work)(const char*)) {
    // TODO: Implement
}

const char* plugin_wait_finished(void) {
    // TODO: Implement
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
    
    if (g_plugin_context.initialized) {
        return "Plugin already exists";
    }
    
    return NULL; 
}

static void free_if_allocated(const char* processed, const char* original) {
    if (processed != NULL && processed != original) {
        free((char*)processed);
    }
}

static void forward_to_next_plugin(plugin_context_t* curr_plugin, const char* str) {
    if (NULL != curr_plugin->next_place_work) {
        const char* error_handler = curr_plugin->next_place_work(str);
        if (NULL != error_handler) {
            log_error(curr_plugin, error_handler);
        }
    }
}