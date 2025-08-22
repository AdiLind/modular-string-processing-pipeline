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
} plugin_handle_t;

// *** Main Helper Function Declarations *** ///
// declare now to use all of them in main skip lazy compilation problems
static void display_usage_help(void); 
static int parse_queue_size_arg(const char* argument_string);
static int load_single_plugin(plugin_handle_t* plugin_handle, const char* plugin_name);
static int extract_plugin_funcs(plugin_handle_t* plugin_handle, const char* plugin_name);
static plugin_handle_t* load_all_plugins(int num_of_plugins, char* plugin_names[]);
static int init_all_plugins(plugin_handle_t* plugins_arr, int num_of_plugins, int queue_size);
static void connect_plugins_in_pipeline_chain(plugin_handle_t* plugins_arr, int num_of_plugins);
static int read_input_and_process(plugin_handle_t* first_plugin_in_chain);
static void free_plugin_resources(plugin_handle_t* plugin_handle);
static void cleanup_all_plugins(plugin_handle_t* plugins_arr, int num_of_plugins);


/// *** Main Function *** ///

///TODO: hello Adi from the future, this is skeleton of main function with all the steps, you need to implement all of those functios
/// dont forget to add those function declarations at the top of this file
/* steps order from the file - 
    //step 1 - parse command line arguments
    //step 2 - load all plugins dynamically and export their functions
    //step 3 - initialize all plugins - construct the pipeline
    //step 4 - connect plugins in a pipeline chain
    //step 5 - read input lines and process them through the pipeline - the main part of the program logic
    //step 6 - graceful shutdown all the plugins - after processing is done or error
    //step 8 - clean up all resources allocated for plugins and the mass we allocated for them
    //step 9 - print exit 
*/
int main(int argc, char* argv[]) 
{
    //step 1 - parse command line arguments
    // echo <string_to_manipulate> | ./analayzer <queue_size> <plugin1> ...
    // program name, queue size, plugin1,.... => min 3 args
    if (argc < 3) 
    {
        fprintf(stderr, "Error: Not enough arguments.\n");
        display_usage_help();
        return EXIT_FAILURE;
    }

    int queue_size_for_plugins = parse_queue_size_arg(argv[1]);
    if(-1 == queue_size_for_plugins)
    {
        fprintf(stderr, "Error: Invalid queue size argument.\n");
        display_usage_help();
        return EXIT_FAILURE;
    }

    int total_num_of_plugins = argc - 2;
    char** plugin_names_from_args = &argv[2];
    
    //step 2 - load all plugins dynamically
    plugin_handle_t* loaded_plugins_arr = load_all_plugins(total_num_of_plugins, plugin_names_from_args);
    if(NULL == loaded_plugins_arr)
    {
        fprintf(stderr, "Error: Failed occur while loading plugins.\n");
        display_usage_help();
        return EXIT_FAILURE;
    }

    //step 3 - initialize all plugins - construct the pipeline
    int init_result = init_all_plugins(loaded_plugins_arr, total_num_of_plugins, queue_size_for_plugins);
    if(-1 == init_result)
    {
        fprintf(stderr, "Error: Failed occur while initializing plugins.\n");
        cleanup_all_plugins(loaded_plugins_arr, total_num_of_plugins);
        return 2; // TODO: define error codes in a header file
    }

    //step 4 - connect plugins in a pipeline chain
    connect_plugins_in_pipeline_chain(loaded_plugins_arr, total_num_of_plugins);

    //step 5 - read input lines and process them through the pipeline - the main part of the program logic
    //read from stdin and send to the first plugin in the chain
    int read_and_processing_result = read_input_and_process(&loaded_plugins_arr[0]);
    if( 0 != read_and_processing_result)
    {
        fprintf(stderr, "Error: Failed occur while reading input and processing.\n");
        cleanup_all_plugins(loaded_plugins_arr, total_num_of_plugins);
        return EXIT_FAILURE;
    }

    //step 6 - graceful shutdown all the plugins - after processing is done or error
    cleanup_all_plugins(loaded_plugins_arr, total_num_of_plugins);
    //step 8 - clean up all resources allocated for plugins and the mass we allocated for them
    //step 9 - print exit 
    printf("Program completed successfully.\n");
    return EXIT_SUCCESS; 
}




// *** implementation helper functions ** /// 

static int parse_queue_size_arg(const char* argument_string) 
{
    if(NULL == argument_string) 
    {
        return EXIT_FAILURE;
    }

    if(argument_string[0] == '\0') 
    {
        return EXIT_FAILURE;
    }

    long parse_result = 0;
    int i = 0;
    //parse each char
    while(argument_string[i] != '\0' && i < MAX_FILE_NAME_LENGTH) 
    {
        char current_char = argument_string[i];
        if(current_char < '0' || current_char > '9') 
        {
            return EXIT_FAILURE;
        }
        int digit = current_char - '0';
        parse_result = parse_result * 10 + digit;
        i++;
    }

    //validate range
    if(parse_result <= 0 || parse_result > 1000000) 
    {
        return EXIT_FAILURE;
    }

    return (int)parse_result;
}

static int load_single_plugin(plugin_handle_t* plugin_handle, const char* plugin_name) 
{
    if (NULL == plugin_handle || NULL == plugin_name) 
    {
        return EXIT_FAILURE;
    }

    char shared_object_file_name[MAX_FILE_NAME_LENGTH]; //TODO: maybe we should add 10 chars for extra space for "lib" prefix and ".so" suffix
    if(strlen(plugin_name) + 8 > MAX_FILE_NAME_LENGTH) // "lib" +".so\0"
    {
        fprintf(stderr, "Error: Plugin name too long: %s\n", plugin_name);
        return EXIT_FAILURE;
    }

    strcpy(shared_object_file_name, plugin_name );
    strcat(shared_object_file_name, ".so");

    //store the copy of the plugin name
    plugin_handle->plugin_name = strdup(plugin_name);
    if(NULL == plugin_handle->plugin_name)
    {
        fprintf(stderr, "failed to allocate memory for plugin name: %s\n", plugin_name);
        return EXIT_FAILURE;
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
    return EXIT_SUCCESS;
}

// extract all the functions from the interface of the plugin
static int extract_plugin_funcs(plugin_handle_t* plugin_handle, const char* plugin_name)
{
    if(NULL == plugin_handle || NULL == plugin_name || NULL == plugin_handle->dynamic_library_handle)
    {
        return EXIT_FAILURE;
    }

    //extract plugin_init
    plugin_handle->init = (plugin_init_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_init");
    if (NULL == plugin_handle->init)
    {
        fprintf(stderr, "Failed to find and extract plugin_init in %s: %s\n", plugin_name, dlerror());
        return EXIT_FAILURE;
    }

    //extract plugin_get_name
    plugin_handle->get_name = (plugin_get_name_func)dlsym(plugin_handle->dynamic_library_handle, "plugin_get_name");
    if (NULL == plugin_handle->get_name) {
        fprintf(stderr, "Failed to find plugin_get_name in %s: %s\n", plugin_name, dlerror());
        return -1;
    }


    //extract plugin_attach
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
    
    //extract plugin_wait_finished
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

    
}