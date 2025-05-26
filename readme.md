# Streamix - High-Performance HTTP File Server

A high-performance HTTP/1.1 server written in modern C++ that efficiently serves large files using zero-copy system calls and modern C++ features. The server implements a subset of HTTP/1.1 with support for GET requests and proper error handling.

## Key Features

- **High Performance**: Utilizes zero-copy `sendfile()` for maximum throughput
- **Multi-threaded**: Handles multiple clients concurrently with thread-per-connection model
- **Resource Safe**: RAII for automatic resource management
- **Modern C++**: Written in C++17 with clean, maintainable code
- **Efficient**: Minimal memory copies and system calls
- **Secure**: Basic HTTP request validation and proper error handling
- **HTTP/1.1**: Implements essential HTTP/1.1 features

## Prerequisites

- Linux (uses Linux-specific system calls like `sendfile`)
- C++17 compatible compiler (g++ 9+ or clang++ 10+ recommended)
- CMake 3.12+ (for building)
- Development tools (make, gcc, etc.)

## Quick Start

```bash
# Build
make

# Generate a 10GB test file
make test-file

# Run the server (requires root for privileged ports < 1024)
make run

# In another terminal, test the server
curl -I http://localhost:8080/
```

### Advanced Usage

```bash
# Custom test file size (e.g., 5GB)
make test-file TEST_SIZE=5

# Clean build artifacts
make clean

# Format code (requires clang-format)
make format

# Build with debug symbols
make debug
```

## Testing

### Basic Test
```bash
# Check server headers using GET
curl -I http://localhost:8080/

# Expected response:
# HTTP/1.1 200 OK
# Content-Length: <file_size_on_server> bytes
# Connection: close
# Content-Type: application/octet-stream
```

### Download Test
```bash
# Download with progress bar
curl --progress-bar -o downloaded_file http://localhost:8080/

# Test with non-GET method
curl -X POST -I http://localhost:8080/
# Expected: 405 Method Not Allowed
```

### Concurrent Connections
```bash
# Test with multiple concurrent connections
for i in {1..10}; do
  curl -s http://localhost:8080/ > /dev/null &
done
wait
```

## Architecture

### Core Components

1. **Socket Class**
   - RAII wrapper for socket operations
   - Handles connection lifecycle
   - Implements move semantics for safe resource transfer

2. **File Class**
   - RAII wrapper for file operations
   - Provides file size information
   - Ensures proper file descriptor cleanup

3. **Thread-per-Connection Model**
   - Creates a new thread for each client connection
   - Uses POSIX threads for handling connections
   - Implements proper resource cleanup in each thread

### Data Flow
1. Server starts and binds to the configured port
2. Main thread accepts incoming connections in a loop
3. For each new connection:
   - Creates a new thread to handle the client
   - The new thread:
     - Sends HTTP headers with proper Content-Length
     - Streams file content using zero-copy `sendfile()`
     - Cleans up resources when done
   - Main thread continues accepting new connections

### Performance Optimizations
- **Zero-copy Transfers**: Uses `sendfile()` for direct file-to-network transfers
- **Efficient Memory**: RAII for automatic resource management
- **Chunked Sending**: Configurable chunk sizes (default: 8MB) for optimal throughput
- **Error Handling**: Robust error handling with proper resource cleanup
- **Threading**: Simple thread-per-connection model (note: for production use, consider a thread pool for better scalability)

## Areas for Improvement

### Testing Infrastructure
- [ ] **Unit Tests**: Add Google Test framework for testing individual components
  - Socket creation and error handling
  - File handling and cleanup
  - HTTP response formatting
- [ ] **Integration Tests**: Test client-server interactions
  - Multiple concurrent connections
  - Partial file transfers
  - Error conditions and recovery

### Logging and Monitoring
- [ ] **Structured Logging**
  - Add timestamps and log levels (INFO, WARN, ERROR)
  - Log client connections and disconnections
  - Track transfer statistics (bytes sent, transfer rate)
- [ ] **Metrics Collection**
  - Track active connections
  - Monitor memory usage
  - Measure request/response times

### Security Enhancements
- [ ] **Authentication**
  - Basic Auth or JWT token validation
  - Rate limiting to prevent abuse
  - IP whitelisting/blacklisting
- [ ] **TLS/HTTPS Support**
  - Add OpenSSL integration
  - Support for HTTP/2
  - Certificate management

### Performance Optimizations
- [ ] **Connection Pooling**
  - Replace thread-per-connection with thread pool
  - Configurable pool size based on CPU cores
  - Queue management for high load
- [ ] **Range Requests**
  - Support HTTP Range headers
  - Enable resumable downloads
  - Handle multiple range requests

### Additional Features
- [ ] **Directory Browsing**
  - List files in a directory
  - Support for index.html fallback
  - File size and modification time display
- [ ] **Graceful Shutdown**
  - Handle SIGTERM/SIGINT
  - Complete ongoing transfers
  - Clean up resources properly
