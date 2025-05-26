# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread

# Source files
SRC := streamix.cpp
TARGET := streamix

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Rebuild from scratch
rebuild: clean all

# Run the server (requires root for port 8080)
run: $(TARGET)
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

.PHONY: all clean rebuild run format
