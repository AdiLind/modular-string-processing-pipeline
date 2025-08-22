#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// This uppercaser plugin transforms input strings to uppercase
static const char* uppercase_transform(const char* input_to_upper)
{
    if(NULL == input_to_upper) {
        return NULL;
    }

    int input_len = strlen(input_to_upper);
    char* after_uppercase = (char*)malloc(input_len + 1);
    if (NULL == after_uppercase) {
        return NULL; // TODO: should we log an error here instead of returning NULL?
    }
    for(int i = 0; i < input_len; i++) {
        after_uppercase[i] = toupper((unsigned char)input_to_upper[i]);
    }
    after_uppercase[input_len] = '\0';//indicate end of string

    return after_uppercase;

}

const char* plugin_init(int queue_size) 
{
    return common_plugin_init(uppercase_transform, "uppercaser", queue_size);
}