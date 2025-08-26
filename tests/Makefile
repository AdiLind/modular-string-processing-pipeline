# Makefile for testing plugins without main.c

CC = gcc
CFLAGS = -Wall -g -fPIC
LDFLAGS = -lpthread
OUTPUT = ../output

# Source files
COMMON_SRCS = ../plugins/plugin_common.c \
              ../plugins/sync/monitor.c \
              ../plugins/sync/consumer_producer.c

PLUGIN_SRCS = ../plugins/logger.c \
              ../plugins/typewriter.c \
              ../plugins/uppercaser.c \
              ../plugins/rotator.c \
              ../plugins/flipper.c \
              ../plugins/expander.c

# Test programs
TESTS = plugin_direct_test interactive_tests

# Default target
all: $(OUTPUT) tests plugins

# Create output directory
$(OUTPUT):
	mkdir -p $(OUTPUT)

# Compile test programs
tests: $(TESTS)

plugin_direct_test: plugin_direct_test.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $(OUTPUT)/$@ $^ $(LDFLAGS)

interactive_tests: interactive_tests.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $(OUTPUT)/$@ $^ $(LDFLAGS)

# Compile plugins as shared objects
plugins: $(OUTPUT)
	@echo "Building plugins as shared objects..."
	@for plugin in logger typewriter uppercaser rotator flipper expander; do \
		echo "  Building $$plugin.so..."; \
		$(CC) $(CFLAGS) -shared -o $(OUTPUT)/$$plugin.so \
			../plugins/$$plugin.c $(COMMON_SRCS) $(LDFLAGS) || exit 1; \
	done
	@echo "All plugins built successfully!"

# Run tests
test: tests
	@echo "=== Running Plugin Tests ==="
	@echo "Running direct tests..."
	@$(OUTPUT)/plugin_direct_test
	@echo ""
	@echo "To run interactive tests:"
	@echo "  make interactive"

# Run interactive test
interactive: interactive_tests
	@$(OUTPUT)/interactive_tests

# Check plugin symbols
check-symbols: plugins
	@echo "=== Checking Plugin Symbols ==="
	@for plugin in logger typewriter uppercaser rotator flipper expander; do \
		echo "Checking $$plugin.so:"; \
		nm -D $(OUTPUT)/$$plugin.so | grep " T plugin_"; \
		echo ""; \
	done

# Clean build artifacts
clean:
	rm -f $(OUTPUT)/plugin_direct_test
	rm -f $(OUTPUT)/interactive_tests
	rm -f $(OUTPUT)/*.so
	rm -f $(OUTPUT)/test_*
	rm -f $(OUTPUT)/*.c

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build all tests and plugins"
	@echo "  tests        - Build test programs"
	@echo "  plugins      - Build plugins as shared objects"
	@echo "  test         - Run automated tests"
	@echo "  interactive  - Run interactive test"
	@echo "  check-symbols- Check exported symbols in plugins"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help message"

.PHONY: all tests plugins test interactive check-symbols clean help