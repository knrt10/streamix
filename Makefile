# Compiler and flags
CXX := g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -pthread

# Source files and target
SRC := streamix.cpp
TARGET := streamix
TEST_FILE := test_file
TEST_SIZE ?= 10  # Default test file size in GB

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Create test file with random data
test-file:
	@echo "Creating test file of size $(TEST_SIZE)GB..."
	@dd if=/dev/urandom of=$(TEST_FILE) bs=1G count=$(TEST_SIZE) status=progress
	@echo "Created $(TEST_FILE) of size $(TEST_SIZE)GB"

# Clean build artifacts and test files
clean:
	rm -f $(TARGET)

# Rebuild from scratch
rebuild: clean all

# Run the server (requires root for port 8080)
run: all
	sudo ./$(TARGET)

# Format the code (requires clang-format)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i $(SRC); \
		echo "Code formatted successfully"; \
	else \
		echo "clang-format not found. Install it with:"; \
		echo "  sudo apt-get install clang-format"; \
		exit 1; \
	fi

.PHONY: all clean rebuild run format test-file
