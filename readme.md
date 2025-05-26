# Streamix - A Simple HTTP File Server

A lightweight HTTP server written in modern C++ that efficiently serves large files using zero-copy system calls.

## Current Status

- Basic TCP server implementation  
- Socket management with RAII  
- Error handling with exceptions  
- Port configuration (default: 8080)  

## Prerequisites

- Linux (uses Linux-specific system calls)
- C++17 compatible compiler (g++ 7+ or clang++ 6+)
- Make (for building)

## Building

```bash
# Get the repository
cd streamix

# Build the project
make all

# Running the server
make run

# Clean up
make clean

# Format the code
make format
```


## How It Works (Current Implementation)

1. Creates a TCP socket and binds it to the specified port
2. Listens for incoming connections
3. Uses RAII for automatic resource cleanup
4. Implements proper error handling with exceptions

## Next Steps

- [ ] Implement HTTP request handling
- [ ] Add file serving via sendfile()
- [ ] Add configuration options
