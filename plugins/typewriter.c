//This typewriter plugin simulates typing by outputting one character at a time with a delay of 100ms

#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TYPEWRITER_CHAR_DELAY_USLEEP 100000 // 100000 => 100ms delay

static const char* typewriter_transform(const char* input_to_type) 
{
    if (NULL == input_to_type) {
        return NULL;
    }
    int input_len = strlen(input_to_type);

    // adding prefix [typewriter]
    const char* prefix = "[typewriter] ";
    fprintf(stdout,"%s", prefix);
    fflush(stdout); //force immediate output

    for(int i=0; i < input_len; i++) {
        fprintf(stdout, "%c", input_to_type[i]);
        fflush(stdout);
        usleep(TYPEWRITER_CHAR_DELAY_USLEEP);
    }

    //add new line after we finish typing the input
    fprintf(stdout, "\n");
    fflush(stdout);

    //move the input to the next plugin in the chain if exists
    char* copy_of_input = strdup(input_to_type);
    if (NULL == copy_of_input)
    {
        return NULL; //TODO: this is necessary ? should we return NULL or error message? how this error could occur?
    }
    return copy_of_input;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(typewriter_transform, "typewriter", queue_size);
}