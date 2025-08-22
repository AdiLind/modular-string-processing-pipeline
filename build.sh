#!/bin/bash

# Exit on any error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Build configuration
OUTPUT_DIR="output"
MAIN_EXECUTABLE="analyzer"
PLUGINS=("logger" "uppercaser" "rotator" "flipper" "expander" "typewriter")

# Compilation flags
CFLAGS="-Wall -Wextra -std=c99 -pthread"
DEBUG_FLAGS="-g -DDEBUG"
RELEASE_FLAGS="-O2 -DNDEBUG"

# Use DEBUG mode if DEBUG environment variable is set
if [ "${DEBUG}" = "1" ]; then
    COMPILE_FLAGS="${CFLAGS} ${DEBUG_FLAGS}"
    print_info "Building in DEBUG mode"
else
    COMPILE_FLAGS="${CFLAGS} ${RELEASE_FLAGS}"
    print_info "Building in RELEASE mode"
fi

print_status "Starting build process..."

# Create output directory
if [ ! -d "${OUTPUT_DIR}" ]; then
    print_status "Creating output directory: ${OUTPUT_DIR}"
    mkdir -p "${OUTPUT_DIR}"
fi

# Clean previous builds
print_status "Cleaning previous builds..."
rm -f "${OUTPUT_DIR}"/*.so "${OUTPUT_DIR}/${MAIN_EXECUTABLE}"

# Build main application
print_status "Building main application: ${MAIN_EXECUTABLE}"
if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/${MAIN_EXECUTABLE}" main.c -ldl -lpthread; then
    print_status "Main application built successfully"
else
    print_error "Failed to build main application"
    exit 1
fi

# Build plugins
print_status "Building plugins..."
for plugin_name in "${PLUGINS[@]}"; do
    print_status "Building plugin: ${plugin_name}"
    
    # Check if plugin source file exists
    if [ ! -f "plugins/${plugin_name}.c" ]; then
        print_error "Plugin source file not found: plugins/${plugin_name}.c"
        exit 1
    fi
    
    # Build plugin
    if gcc -fPIC -shared ${COMPILE_FLAGS} \
        -o "${OUTPUT_DIR}/${plugin_name}.so" \
        "plugins/${plugin_name}.c" \
        "plugins/plugin_common.c" \
        "plugins/sync/monitor.c" \
        "plugins/sync/consumer_producer.c" \
        -ldl -lpthread; then
        print_status "Plugin ${plugin_name} built successfully"
    else
        print_error "Failed to build plugin: ${plugin_name}"
        exit 1
    fi
done

# Build test applications (if they exist)
print_status "Building test applications..."
test_apps=()

if [ -f "tests/monitor_comprehensive_test.c" ]; then
    print_status "Building monitor comprehensive test"
    if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/test_monitor_comprehensive" \
        "tests/monitor_comprehensive_test.c" \
        "plugins/sync/monitor.c" \
        -lpthread; then
        test_apps+=("test_monitor_comprehensive")
        print_status "Monitor comprehensive test built"
    else
        print_warning "Failed to build monitor comprehensive test"
    fi
fi

if [ -f "tests/consumer_producer_test.c" ]; then
    print_status "Building consumer-producer test"
    if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/test_consumer_producer" \
        "tests/consumer_producer_test.c" \
        "plugins/sync/consumer_producer.c" \
        "plugins/sync/monitor.c" \
        -lpthread; then
        test_apps+=("test_consumer_producer")
        print_status "Consumer-producer test built"
    else
        print_warning "Failed to build consumer-producer test"
    fi
fi

if [ -f "tests/plugin_infra_test.c" ]; then
    print_status "Building plugin infrastructure test"
    if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/test_plugin_infra" \
        "tests/plugin_infra_test.c" \
        "plugins/plugin_common.c" \
        "plugins/sync/consumer_producer.c" \
        "plugins/sync/monitor.c" \
        -lpthread; then
        test_apps+=("test_plugin_infra")
        print_status "Plugin infrastructure test built"
    else
        print_warning "Failed to build plugin infrastructure test"
    fi
fi

if [ -f "tests/plugin_direct_test.c" ]; then
    print_status "Building plugin direct test"
    if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/test_plugin_direct" \
        "tests/plugin_direct_test.c" \
        "plugins/plugin_common.c" \
        "plugins/sync/consumer_producer.c" \
        "plugins/sync/monitor.c" \
        -lpthread; then
        test_apps+=("test_plugin_direct")
        print_status "Plugin direct test built"
    else
        print_warning "Failed to build plugin direct test"
    fi
fi

# Build simple ping-pong test if it exists
if [ -f "tests/simple_ping_pong_test.c" ]; then
    print_status "Building simple ping-pong test"
    if gcc ${COMPILE_FLAGS} -o "${OUTPUT_DIR}/test_ping_pong" \
        "tests/simple_ping_pong_test.c" \
        "plugins/sync/monitor.c" \
        -lpthread; then
        test_apps+=("test_ping_pong")
        print_status "Simple ping-pong test built"
    else
        print_warning "Failed to build simple ping-pong test"
    fi
fi

# Verify builds
print_status "Verifying builds..."

# Check main executable
if [ -x "${OUTPUT_DIR}/${MAIN_EXECUTABLE}" ]; then
    print_status "Main executable verified"
else
    print_error "Main executable not found or not executable"
    exit 1
fi

# Check plugins
plugin_count=0
for plugin_name in "${PLUGINS[@]}"; do
    if [ -f "${OUTPUT_DIR}/${plugin_name}.so" ]; then
        plugin_count=$((plugin_count + 1))
    else
        print_error "Plugin ${plugin_name}.so not found"
        exit 1
    fi
done

print_status "All ${plugin_count} plugins verified"

# List test applications built
if [ ${#test_apps[@]} -gt 0 ]; then
    print_status "Test applications built: ${test_apps[*]}"
else
    print_warning "No test applications built"
fi

# Final summary
print_status "Build completed successfully!"
print_info "Built files:"
print_info "  - Main executable: ${OUTPUT_DIR}/${MAIN_EXECUTABLE}"
print_info "  - Plugins (${#PLUGINS[@]}): ${PLUGINS[*]}"
if [ ${#test_apps[@]} -gt 0 ]; then
    print_info "  - Test apps (${#test_apps[@]}): ${test_apps[*]}"
fi

print_status "Ready to run tests!"