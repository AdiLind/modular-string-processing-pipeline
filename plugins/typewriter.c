//This typewriter plugin simulates typing by outputting one character at a time 
// with a delay of 100ms (configurable via TYPEWRITER_CHAR_DELAY_USLEEP).

#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TYPEWRITER_CHAR_DELAY_USLEEP 100000

static const char* typewriter_transform(const char* input_to_type) 
{
    if (NULL == input_to_type) {
        return NULL;
    }
    int input_len = strlen(input_to_type);


    const char* prefix = "[typewriter] ";
    int prefix_len = strlen(prefix);
    
    // Type prefix character by character with delay
    for(int i=0; i < prefix_len; i++) {
        fprintf(stdout, "%c", prefix[i]);
        fflush(stdout);
        usleep(TYPEWRITER_CHAR_DELAY_USLEEP);
    }

    // Type input character by character with delay
    for(int i=0; i < input_len; i++) {
        fprintf(stdout, "%c", input_to_type[i]);
        fflush(stdout);
        usleep(TYPEWRITER_CHAR_DELAY_USLEEP);
    }

    //add new line after we finish typing the input
    fprintf(stdout, "\n");
    fflush(stdout);

    //move the input to the next plugin in the chain if exists
    size_t len = strlen(input_to_type);
    char* copy_of_input = (char*)malloc(len + 1);
    if (NULL == copy_of_input) { return NULL; }

    if (copy_of_input) {
        strcpy(copy_of_input, input_to_type);
    }

    return copy_of_input;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(typewriter_transform, "typewriter", queue_size);
}