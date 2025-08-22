// this plugin flip the string order like reverse

#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>

static const char* flipper_transform(const char* input_to_flip)
{
    if (NULL == input_to_flip) {
        return NULL;
    }

    int input_len = strlen(input_to_flip);
    //handle empty string case, return empty string as well
    if (0 == input_len) {
        return strdup(""); 
    }

    char* after_flipping_result = (char*)malloc(input_len + 1);
    if (NULL == after_flipping_result)
    {
        return NULL; // TODO: should we log an error here instead of returning NULL?
    }

    for (int i = 0; i < input_len; i++)
    {
        after_flipping_result[i] = input_to_flip[input_len - 1 - i];
    }

    after_flipping_result[input_len] = '\0'; //add the terminal of end string

    return after_flipping_result;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(flipper_transform, "flipper", queue_size);
}
