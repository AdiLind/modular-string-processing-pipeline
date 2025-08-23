#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>

//this plugin move every char one position to the right in circular manner (the last becomes the first)
static const char* rotator_transform(const char* input_to_rotate)
{
    if (NULL == input_to_rotate) {
        return NULL;
    }

    int input_len = strlen(input_to_rotate);
    //handle empty string case - return empty string
    if (0 == input_len) {
        char* result = (char*)malloc(1);
        if (result) {
            result[0] = '\0';
        }
        return result; 
    }

    char* after_rotation_result = (char*)malloc(input_len + 1);
    if (NULL == after_rotation_result)
    {
        return NULL; // TODO: should we log an error here instead of returning NULL?
    }

    after_rotation_result[0] = input_to_rotate[input_len - 1]; //insert the lastchar at the beginning
    for (int i = 1; i < input_len; i++) {
        after_rotation_result[i] = input_to_rotate[i - 1];
    }
    after_rotation_result[input_len] = '\0'; // indicate end of string

    return after_rotation_result;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(rotator_transform, "rotator", queue_size);
}