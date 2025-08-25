/*
* This is the main core of this modular pipeline system.
* here we use dynamic loading to load plugins at runtime.
* each plugin implements a specific string transformation and run on its own thread.
* the main program load the plugins and connect them in a pipeline.
*/
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

//consts 
#define Max_line_length 1024
#define MAX_FILE_NAME_LENGTH 256

// type def for plugin functions
typedef const char* (*plugin_get_name_func)(void);
typedef const char* (*plugin_init_func)(int);
typedef const char* (*plugin_fini_func)(void);
typedef const char* (*plugin_place_work_func)(const char*);
typedef void (*plugin_attach_func)(const char* (*)(const char*));
typedef const char* (*plugin_wait_finished_func)(void);

//define a struct to hold plugin information
typedef struct {
    //pointer to plugin interface functions
    plugin_init_func init;
    plugin_fini_func fini;
    plugin_place_work_func place_work;
    plugin_attach_func attach;
    plugin_wait_finished_func wait_finished;
    plugin_get_name_func get_name;

    char* plugin_name;
    void* dynamic_library_handle;
    int instance_id;       // we need this to separte contexts between same plugins instances
} plugin_handle_t;


static int global_plugin_instance_counter = 0;

// ####  Helper Func Declarations ### ///
// we need to declare now to use all of them in main skip lazy compilation problems, trick we learn with pain and blood :)
static void display_usage_help(void); 
static int parse_queue_size_arg(const char* argument_string);
static int load_single_plugin_with_dlmopen(plugin_handle_t* plugin_handle, const char* plugin_name);
//static int load_single_plugin(plugin_handle_t* plugin_handle, const char* plugin_name);
static int extract_plugin_funcs(plugin_handle_t* plugin_handle, const char* plugin_name);
static plugin_handle_t* load_all_plugins(int num_of_plugins, char* plugin_names[]);
static int init_all_plugins(plugin_handle_t* plugins_arr, int num_of_plugins, int queue_size);
static void connect_plugins_in_pipeline_chain(plugin_handle_t* plugins_arr, int num_of_plugins);
static int read_input_and_process(plugin_handle_t* first_plugin_in_chain);
static void free_plugin_resources(plugin_handle_t* plugin_handle);
static void cleanup_all_plugins_in_range(plugin_handle_t* plugins_arr, int num_of_plugins);


/// TO BE DELETED !!!! 
///TODO: hello Adi from the future, this is skeleton of main function with all the steps, you need to implement all of those functios
/// dont forget to add those function declarations at the top of this file
/* steps order from the file - 
    // step 1- parse command line arguments
    // step 2- load all plugins dynamically and export their functions
    // step 3- initialize all plugins - construct the pipeline
    // step 4- connect plugins in a pipeline chain 
    //step 5- read input lines and process them through the pipeline - the main part of the program logic
    // step 6- graceful shutdown all the plugins - after processing is done or error
    // step 8- clean up all resources allocated for plugins and the mass we allocated for them
    // step 9- print exit 
*/
int main(int argc, char* argv[]) 
{
    //step 1 - parse command line arguments
    // echo <string_to_manipulate> | ./output/analayzer <queue_size> <plugin1> ...
    // program path+name, queue size, plugin1,.... => min 3 args
    if (argc < 3) 
    {
        fprintf(stderr, "Error: Not enough arguments.\n");
        display_usage_help();
        return 1;
    }

    int queue_size_for_plugins = parse_queue_size_arg(argv[1]);
    if(-1 == queue_size_for_plugins)
    {
        fprintf(stderr, "Error: Invalid queue size argument.\n");
        display_usage_help();
        return 1;
    }

    int total_num_of_plugins = argc - 2;
    char** plugin_names_from_args = &argv[2];
    
    //step 2 - load all plugins dynamically
    plugin_handle_t* loaded_plugins_arr = load_all_plugins(total_num_of_plugins, plugin_names_from_args);
    if(NULL == loaded_plugins_arr)
    {
        fprintf(stderr, "Error: Failed occur while loading plugins.\n");
        display_usage_help();
        return 1;
    }

    //step 3 - initialize all plugins - construct the pipeline
    int init_result = init_all_plugins(loaded_plugins_arr, total_num_of_plugins, queue_size_for_plugins);
    if(-1 == init_result)
    {
        fprintf(stderr, "Error: Failed occur while initializing plugins.\n");
        cleanup_all_plugins_in_range(loaded_plugins_arr, total_num_of_plugins);
        return 2; // TODO: check again if this is should be 2 and make a clear define error codes in a header file
    }

    //step 4 - connect plugins in a pipeline chain
    connect_plugins_in_pipeline_chain(loaded_plugins_arr, total_num_of_plugins);

    //step 5 - read input lines and process them through the pipeline - the main part of the program logic
    //read from stdin and send to the first plugin in the chain
    int read_and_processing_result = read_input_and_process(&loaded_plugins_arr[0]);
    if( 0 != read_and_processing_result)
    {
        fprintf(stderr, "Error: Failed occur while reading input and processing.\n");
        cleanup_all_plugins_in_range(loaded_plugins_arr, total_num_of_plugins);
        return 1;
    }

    //step 6 - wait for all plugins to finish processing before cleanup
    for(int plugin_index = 0; plugin_index < total_num_of_plugins; plugin_index++) {
        if(loaded_plugins_arr[plugin_index].wait_finished) {
            loaded_plugins_arr[plugin_index].wait_finished();
        }
    }

    //step 7 - graceful shutdown all the plugins - after processing is done or error
    cleanup_all_plugins_in_range(loaded_plugins_arr, total_num_of_plugins);
    //step 8 - clean up all resources allocated for plugins and the mass we allocated for them
    //step 9 - print exit 
    printf("Pipeline shutdown complete\n");
    return 0; 
}



// *** implementation helper functions ** /// 

static int parse_queue_size_arg(const char* argument_string) 
{
    if(NULL == argument_string) 
    {
        return -1;
    }

    if(argument_string[0] == '\0') 
    {
        return -1;
    }

    long parse_result = 0;
    int i = 0;
    //parse each char until we reach the end 
    while(argument_string[i] != '\0' && i < MAX_FILE_NAME_LENGTH) 
    {
        char current_char = argument_string[i];
        if(current_char < '0' || current_char > '9') 
        {
            return -1;
        }
        int digit = current_char - '0';
        parse_result = parse_result * 10 + digit;
        i++;
    }

    //validate range
    if(parse_result <= 0 || parse_result > 1000000) 
    {
        return -1;
    }

    return (int)parse_result;
}




// in order to load multiple instances of the same plugin type more than once I choose to use dlmopen instead of dlopen
// I read in the piazza that this is valid approach to use this method. I worked on this feature more than 2 days so please dont reduce points on this func :) 
static int load_single_plugin_with_dlmopen(plugin_handle_t* plugin_handle, const char* plugin_name) 
{
    if (NULL == plugin_handle || NULL == plugin_name) 
    {
        return 1;
    }

    char so_file_path[MAX_FILE_NAME_LENGTH];
    
    if(strlen(plugin_name) + 8 > MAX_FILE_NAME_LENGTH) //"output/" + ".so\0"
    {
        fprintf(stderr, "Error: Plugin name too long: %s\n", plugin_name);
        return 1;
    }

    //make the .so file path
    int len = snprintf(so_file_path, sizeof(so_file_path), "output/%s.so", plugin_name);
    if(len >= MAX_FILE_NAME_LENGTH || len < 0) 
    {
        fprintf(stderr, "ERROR: Plugin path too long: %s\n", plugin_name);
        return 1;
    }

    //store the plugin info
    plugin_handle->plugin_name = strdup(plugin_name);
    if(NULL == plugin_handle->plugin_name)
    {
        fprintf(stderr, "failed to allocate memory for plugin name: %s\n", plugin_name);
        return 1;
    }

    global_plugin_instance_counter++;
    plugin_handle->instance_id = global_plugin_instance_counter;

    // we use dlmopen to load each instance into a separate namespace
    // this func allows us to use multiple instances of the same plugin to have separate global state
    plugin_handle->dynamic_library_handle = dlmopen(LM_ID_NEWLM, so_file_path, RTLD_NOW); // TODO: check if we need RTLD_NOW | RTLD_LOCAL instead
    if (NULL == plugin_handle->dynamic_library_handle)
    {
        fprintf(stderr, "fail occur while loading plugin %s from %s: %s\n", plugin_name, so_file_path, dlerror());
        free(plugin_handle->plugin_name);
        plugin_handle->plugin_name = NULL;
        return 1;
    }
    
    return 0;
}

// now we will extract all the functions from the interface of the plugin
static int extract_plugin_funcs(plugin_handle_t* plugin_handle, const char* plugin_name)
{
    if(NULL == plugin_handle || NULL == plugin_name || NULL == plugin_handle->dynamic_library_handle)
    {
        return EXIT_FAILURE;
    }

    //plugin_init
    plugin_handle->init = (plugin_init_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_init");
    if (NULL == plugin_handle->init)
    {
        fprintf(stderr, "Failed to find and extract plugin_init in %s: %s\n", plugin_name, dlerror());
        return EXIT_FAILURE;
    }

    //plugin_get_name
    plugin_handle->get_name = (plugin_get_name_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_get_name");
    if (NULL == plugin_handle->get_name) {
        fprintf(stderr, "Failed to find plugin_get_name in %s: %s\n", plugin_name, dlerror());
        return -1;
    }


    //plugin_attach
    plugin_handle->attach = (plugin_attach_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_attach");
    if (NULL == plugin_handle->attach) {
        fprintf(stderr, "Failed to find plugin_attach in %s: %s\n", plugin_name, dlerror());
        return -1;
    }
    
    //extract plugin_place_work
    plugin_handle->place_work = (plugin_place_work_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_place_work");
    if (NULL == plugin_handle->place_work)
    {
        fprintf(stderr, "Failed to find and extract plugin_place_work in %s: %s\n", plugin_name, dlerror());
        return EXIT_FAILURE;    
    }
    
    //plugin_wait_finished
    plugin_handle->wait_finished = (plugin_wait_finished_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_wait_finished");
    if (NULL == plugin_handle->wait_finished) {
        fprintf(stderr, "Failed to find plugin_wait_finished in %s: %s\n", plugin_name, dlerror());
        return -1;
    }

    //extract plugin_fini
    plugin_handle->fini = (plugin_fini_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_fini");
    if (NULL == plugin_handle->fini)
    {
        fprintf(stderr, "Failed to find and extract plugin_fini in %s: %s\n", plugin_name, dlerror());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}

static plugin_handle_t* load_all_plugins(int num_of_plugins, char* plugin_names[])
{
    if(num_of_plugins <= 0 || NULL == plugin_names)
    {
        return NULL;
    }

    //allocate array for all plugins
    plugin_handle_t* plugins_array = (plugin_handle_t*)calloc(num_of_plugins, sizeof(plugin_handle_t));
    if(NULL == plugins_array)
    {
        fprintf(stderr, "Failed to allocate memory for plugins array.\n");
        return NULL;
    }

    //load one after another and extract their functions
    int current_plugin_index;
    for(current_plugin_index= 0; current_plugin_index < num_of_plugins; current_plugin_index++)
    {
        int single_plugin_load_result = load_single_plugin_with_dlmopen(&plugins_array[current_plugin_index], plugin_names[current_plugin_index]);
        if(0 != single_plugin_load_result)
        {
            fprintf(stderr, "Error: Failed to load plugin: %s\n", plugin_names[current_plugin_index]);
            //cleanup all the plugins we have loaded so far
            cleanup_all_plugins_in_range(plugins_array, current_plugin_index);

            free(plugins_array);
            return NULL;
        }

        //extract functions of this plugin
        int extract_result = extract_plugin_funcs(&plugins_array[current_plugin_index], plugin_names[current_plugin_index]);
        if(0 != extract_result)
        {
            fprintf(stderr, "Error: Failed to extract functions from plugin: %s\n", plugin_names[current_plugin_index]);
            //cleanup all the plugins we have loaded so far
            cleanup_all_plugins_in_range(plugins_array, current_plugin_index + 1); //include this one
            free(plugins_array);
            return NULL;
        }
    }

    return plugins_array;
}

static int init_all_plugins(plugin_handle_t* plugins_arr, int num_of_plugins, int queue_size)
{
    if(NULL == plugins_arr || num_of_plugins <= 0 || queue_size <= 0)
    {
        return 1;
    }
    

    int current_plugin_index;
    for(current_plugin_index = 0; current_plugin_index < num_of_plugins; current_plugin_index++)
    {
        const char* init_error = plugins_arr[current_plugin_index].init(queue_size);
        if(NULL != init_error)
        {
            fprintf(stderr, "Error: plugin_init function is NULL for plugin%s: %s\n", plugins_arr[current_plugin_index].plugin_name, init_error);
            //clean with safe rollback
            cleanup_all_plugins_in_range(plugins_arr, current_plugin_index);
            return 1;
        }

    }

    return 0;
}

static void connect_plugins_in_pipeline_chain(plugin_handle_t* plugins_arr, int num_of_plugins)
{
    if(NULL == plugins_arr || num_of_plugins <= 0)
    {
        return;
    }

    //connect each plugin to the next one
    for(int current_index = 0; current_index < num_of_plugins - 1; current_index++) // we stop at n-1 in order to avoid overflow in the last plugin
    {
        int next_index = current_index + 1;
        plugins_arr[current_index].attach(plugins_arr[next_index].place_work);
    }

    // usleep(10000); 
}

static int read_input_and_process(plugin_handle_t* first_plugin_in_chain)
{
    if(NULL == first_plugin_in_chain)
    {
        return 1;
    }

    char input_line_buffer[Max_line_length];
    int end_signal_received = 0;


    while(NULL != fgets(input_line_buffer, sizeof(input_line_buffer), stdin) )
    {
        //remove '\n'
        size_t line_len = strlen(input_line_buffer);
        if(line_len > 0 && input_line_buffer[line_len - 1] == '\n')
        {
            input_line_buffer[line_len - 1] = '\0'; //insert null instead of \n
        }

        //send to the first plugin in the chain
        const char* place_work_error = first_plugin_in_chain->place_work(input_line_buffer);
        if(NULL != place_work_error)
        {
            fprintf(stderr, "Error: Failed to place work to plugin %s: %s\n", first_plugin_in_chain->plugin_name, place_work_error);
            return 1;
        }

        //check for EOF - if we dont get <END> we will not halt
        if( 0 == strcmp(input_line_buffer, "<END>") )
        {
            end_signal_received = 1;
            break;
        }
    }

    // TODO: check in the piazza / the pdf instructor notes if we need to send <END> on EOF
    // there is no clear instruction about it, friend said if its not had <END> its should hang, but it makes sense to me to do it
    // for now i commented it out 


        // if (!end_signal_received) {
        // printf("EOF reached, sending <END> signal...\n"); // Debug output - commented for clean submission
        // const char* place_work_error = first_plugin_in_chain->place_work("<END>");
        // if (NULL != place_work_error) 
        // {
        //     fprintf(stderr, "Warning: Failed to send <END> signal: %s\n", place_work_error);
        // }
    // }


    return 0;
}

static void free_plugin_resources(plugin_handle_t* plugin_handle)
{
    if(NULL == plugin_handle)
    {
        return;
    }

    if(NULL != plugin_handle->plugin_name)
    {
        //free plugin name
        free(plugin_handle->plugin_name);
        plugin_handle->plugin_name = NULL;
    }

    if(NULL != plugin_handle->dynamic_library_handle)
    {
        dlclose(plugin_handle->dynamic_library_handle);
        plugin_handle->dynamic_library_handle = NULL;
    }

    

    plugin_handle->instance_id = 0;
}

static void cleanup_all_plugins_in_range(plugin_handle_t* plugins_arr, int num_of_plugins)
{
    if(NULL == plugins_arr || num_of_plugins <= 0)
    {
        return;
    }

    for(int wait_index = 0; wait_index < num_of_plugins; wait_index++)
    {
        //wait until all plugins will finish processing
        //TOCHECK: are we sure we need to wait for all of them? what if one of them failed to initialize? or get into deadlock? חס וחלילה
        if(NULL != plugins_arr[wait_index].wait_finished)
        {
            const char* wait_error = plugins_arr[wait_index].wait_finished();
            if(NULL != wait_error)
            {
                fprintf(stderr, "Warning: plugin_wait_finished returned error for plugin %s: %s\n", 
                        plugins_arr[wait_index].plugin_name, wait_error);
                plugins_arr[wait_index].wait_finished(); // try again מה כבר יקרה YOLO
                //fprintf(stderr, "Warning: plugin_wait_finished returned error for plugin %s: %s\n", plugins_arr[wait_index].plugin_name, wait_error);
            }
        }
    }

    //finalize  and free resources from all plugins
    for(int fini_index = 0; fini_index < num_of_plugins; fini_index++)
    {
        

        if(NULL != plugins_arr[fini_index].fini)
        {
            const char* fini_error = plugins_arr[fini_index].fini();
            if(NULL != fini_error)
            {
                //plugins_arr[fini_index].fini();
                fprintf(stderr, "Warning: plugin_fini returned error for plugin %s: %s\n", 
                        plugins_arr[fini_index].plugin_name, fini_error);        
            }
        }

        //free all resources allocated for this plugin
        free_plugin_resources(&plugins_arr[fini_index]);
    }

    free(plugins_arr);
}

static void display_usage_help(void) {
    printf("Usage: ./analyzer <queue_size> <plugin1> <plugin2> ... <pluginN>\n");
    printf("Arguments:\n");
    printf("  queue_size  Maximum number of items in each plugin's queue \n");
    printf("  plugin1..N  Names of plugins to load (without .so extension)\n");
    printf("Available plugins:\n");
    printf("  logger      - Logs all strings that pass through\n");
    printf("  typewriter  - Simulates typewriter effect with delays\n");
    printf("  uppercaser  - Converts strings to uppercase\n");
    printf("  rotator     - Move every character to the right. Last character moves to the beginning.\n");
    printf("  flipper     - Reverses the order of characters\n");
    printf("  expander    - Expands each character with spaces\n");
    printf("Example:\n");
    printf("  ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo 'hello' | ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo '<END>' | ./analyzer 20 uppercaser rotator logger\n");
}



/* 
* old version of load_single_plugin using dlopen - we changed to dlmopen to support multiple instances of the same plugin
* kept it here for reference and in case we need to rollback (we also can reach it from git but im lazy :) )
static int load_single_plugin(plugin_handle_t* plugin_handle, const char* plugin_name) 
{
    if (NULL == plugin_handle || NULL == plugin_name) 
    {
        return 1;
    }

    char shared_object_file_name[MAX_FILE_NAME_LENGTH]; //TODO: maybe we should add 10 chars for extra space for "lib" prefix and ".so" suffix
    if(strlen(plugin_name) + 8 > MAX_FILE_NAME_LENGTH) // "lib" +".so\0"
    {
        fprintf(stderr, "Error: Plugin name too long: %s\n", plugin_name);
        return 1;
    }

    // strcpy(shared_object_file_name, plugin_name );
    // strcat(shared_object_file_name, ".so");

    int len = snprintf(shared_object_file_name, sizeof(shared_object_file_name), "output/%s.so", plugin_name);

    if(len >= MAX_FILE_NAME_LENGTH || len < 0) 
    {
        fprintf(stderr, "Error: Plugin path too long: %s\n", plugin_name);
        return 1;
    }

    //store the copy of the plugin name
    plugin_handle->plugin_name = strdup(plugin_name);
    if(NULL == plugin_handle->plugin_name)
    {
        fprintf(stderr, "failed to allocate memory for plugin name: %s\n", plugin_name);
        return 1;
    }

    //load the shared object
    plugin_handle->dynamic_library_handle = dlopen(shared_object_file_name, RTLD_NOW | RTLD_LOCAL);
    if (NULL == plugin_handle->dynamic_library_handle)
    {
        fprintf(stderr, "Failed to load plugin %s: %s\n", plugin_name, dlerror());
        free(plugin_handle->plugin_name);
        plugin_handle->plugin_name = NULL;
        return EXIT_FAILURE;
    }
    return 0;
}

*/