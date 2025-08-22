#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//This logger plugin simply logs the strings it receives to stdout with [logger] prefix
static const char* logger_transform(const char* input_to_log) 
{
    if (NULL == input_to_log) {
        return NULL;
    }
    // Print to stdout with [logger] prefix
    fprintf(stdout, "[logger] %s", input_to_log);
    fflush(stdout);
    
    // Return a copy of the string for the next plugin
    char* copy_of_log_input = strdup(input_to_log);
    if (NULL == copy_of_log_input) {
        return NULL;
    }
    
    return copy_of_log_input;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(logger_transform, "logger", queue_size);
}