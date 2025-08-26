#!/bin/bash

set -e  #Exit with any error

# Colors for output
# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color


# Function to print colored output
print_status()
{
 echo -e "${GREEN}[BUILD]${NC} $1"
}
print_warning()
{
 echo -e "${YELLOW}[WARNING]${NC} $1"
}
print_error()
{
 echo -e "${RED}[ERROR]${NC} $1"
}

print_status "Starting build process..."

# check if there is the output directory - if not, we create it
if [ ! -d "output" ]; then
    print_status "Creating output directory: output"
    mkdir -p output # -p is like to do create if not exists without errors
fi

# Clean previous builds - the analyzer and all plugins
print_status "Cleaning previous builds..."
rm -f output/*.so output/analyzer

# now we can compile the main app
print_status "Compiling main application..."
gcc -o output/analyzer main.c -ldl -lpthread
#check the exit code of the last command
# if [ $? -eq 0 ]; then
#     print_status "Main application built successfully"
# else
#     print_error "Failed to build main application"
#     exit 1
# fi

## we had set -e at the start, so if the build failed we didnt reach this point
print_status "Main application built successfully"


# now we can compile all plugins - we use the code from the pdf instructions
print_status "Start building plugins..."
for plugin_name in logger uppercaser rotator flipper expander typewriter; do

    print_status "Building plugin: $plugin_name"
    gcc -fPIC -shared -o output/${plugin_name}.so \
        plugins/${plugin_name}.c \
        plugins/plugin_common.c \
        plugins/sync/monitor.c \
        plugins/sync/consumer_producer.c \
        -ldl -lpthread || {
        print_error "Failed to build $plugin_name"
        exit 1
    }
    #print_status "Plugin $plugin_name built successfully"
done

print_status "All plugins built successfully"
print_status "Built files:"
print_status "  - Main executable: output/analyzer"
print_status "  - Plugins: logger.so uppercaser.so rotator.so flipper.so expander.so typewriter.so"
