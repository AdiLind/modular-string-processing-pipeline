// This plugin insert a string between every two characters of the input string
#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>

static const char* expander_transform(const char* input_to_expand)
{
    if (NULL == input_to_expand) {
        return NULL;
    }

    int input_len = strlen(input_to_expand);
    //handle empty string case - return empty string
    if (0 == input_len) {
        return strdup(""); 
    }

    
    int new_len_after_expanded = input_len * 2 - 1; //[char and space] + only char without space at the end
    char* after_expanding_result = (char*)malloc(new_len_after_expanded + 1);
    if (NULL == after_expanding_result)
    {
        return NULL; // TODO: should we log an error here instead of returning NULL?
    }

    for (int i = 0; i < input_len; i++) {
        after_expanding_result[i * 2] = input_to_expand[i]; //set the origin char in the even index
        if (i < input_len - 1) 
        {
            after_expanding_result[i * 2 + 1] = ' '; //set the spaces between those  chars
        }
    }
    after_expanding_result[new_len_after_expanded] = '\0';

    return after_expanding_result;
}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(expander_transform, "expander", queue_size);
}