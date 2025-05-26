/**
 * @file streamix.cpp
 * @brief High-performance file streaming server implementation
 *
 * This server efficiently streams files over HTTP using sendfile() for
 * zero-copy transfers and handles multiple concurrent clients using POSIX
 * threads.
 */

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string_view>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

// Server configuration
namespace config {
// TODO: Make this configurable either using environment variables or command
// line arguments
constexpr int PORT = 8080; ///< Default server port
// Make sure to run `make test-file` to create the test file
constexpr std::string_view FILE_PATH = "./test_file";
constexpr size_t SEND_CHUNK_SIZE = 8 * 1024 * 1024; ///< 8MB chunks for sendfile
} // namespace config

/**
 * @brief Handle fatal errors by throwing a system_error
 * @param msg Descriptive error message
 * @throws std::system_error with the current errno value
 */
[[noreturn]] void handle_error(const std::string &msg) {
  throw std::system_error(errno, std::generic_category(), msg);
}

/**
 * @brief Client connection information
 */
struct ClientInfo {
  int fd;         ///< Client socket file descriptor
  std::string ip; ///< Client IP address in string format
  uint16_t port;  ///< Client port number in host byte order

  /**
   * @brief Construct a new ClientInfo object
   * @param fd_ File descriptor of the client socket
   * @param ip_ Client IP address
   * @param port_ Client port number
   */
  ClientInfo(int fd_, std::string ip_, uint16_t port_)
      : fd(fd_), ip(std::move(ip_)), port(port_) {}
};

// Socket class for RAII management of socket descriptors
class Socket {
  int fd_ = -1;

public:
  /**
   * @brief Construct a new Socket object
   * @param domain Communication domain (e.g., AF_INET for IPv4)
   * @param type Socket type (e.g., SOCK_STREAM for TCP)
   * @param protocol Protocol to use (0 for default)
   * @throws std::system_error if socket creation fails
   */
  explicit Socket(int domain, int type, int protocol = 0) {
    fd_ = socket(domain, type, protocol);
    if (fd_ < 0) {
      handle_error("socket creation failed");
    }
  }

  // Clean up socket on destruction
  ~Socket() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  // Prevent copying to avoid double close
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // Allow moving
  Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  // Move assignment
  Socket &operator=(Socket &&other) noexcept {
    if (this != &other) {
      if (fd_ >= 0)
        close(fd_);
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  // Enable address reuse (useful for quick restarts)
  void set_reuse_addr(bool on = true) {
    int optval = on ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
        0) {
      handle_error("setsockopt(SO_REUSEADDR) failed");
    }
  }

  // Bind socket to address
  void bind(const sockaddr *addr, socklen_t addr_len) {
    if (::bind(fd_, addr, addr_len) < 0) {
      handle_error("bind() failed");
    }
  }

  // Start listening for connections
  void listen(int backlog = SOMAXCONN) {
    if (::listen(fd_, backlog) < 0) {
      handle_error("listen() failed");
    }
  }

  // Accept a new client connection
  ClientInfo accept() {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    int client_fd =
        ::accept(fd_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);
    if (client_fd < 0) {
      handle_error("accept() failed");
    }

    // Convert client IP to string
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    return {client_fd, client_ip, ntohs(client_addr.sin_port)};
  }
};

// Create and configure a server socket
Socket create_server_socket() {
  // Create TCP socket
  Socket sock(AF_INET, SOCK_STREAM);
  sock.set_reuse_addr(true);

  // Configure server address
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(config::PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind and listen
  sock.bind(reinterpret_cast<const sockaddr *>(&server_addr),
            sizeof(server_addr));
  sock.listen();

  printf("Server listening on port %d\n", config::PORT);
  return sock;
}

/**
 * @brief RAII wrapper for file operations
 *
 * This class manages file resources and provides safe access to file
 * operations. It ensures proper cleanup of file descriptors and provides file
 * size information.
 */
class File {
  int fd_ = -1;    ///< File descriptor (-1 if invalid)
  off_t size_ = 0; ///< Size of the file in bytes

public:
  /**
   * @brief Construct a new File object and open the specified file
   * @param path Path to the file to open
   * @throws std::system_error if the file cannot be opened or its size cannot
   * be determined
   */
  explicit File(const char *path) {
    fd_ = open(path, O_RDONLY);
    if (fd_ < 0) {
      handle_error("open() failed");
    }

    // Get file stats
    struct stat st;
    if (fstat(fd_, &st) < 0) {
      close(fd_);
      handle_error("fstat() failed");
    }
    size_ = st.st_size;
  }

  /**
   * @brief Destroy the File object and close the file descriptor
   */
  ~File() {
    if (fd_ >= 0)
      close(fd_);
  }

  // Prevent copying
  File(const File &) = delete;
  File &operator=(const File &) = delete;

  // Allow moving
  File(File &&other) noexcept : fd_(other.fd_), size_(other.size_) {
    other.fd_ = -1;
  }

  // Move assignment
  File &operator=(File &&other) noexcept {
    if (this != &other) {
      if (fd_ >= 0)
        close(fd_);
      fd_ = other.fd_;
      size_ = other.size_;
      other.fd_ = -1;
    }
    return *this;
  }

  /**
   * @brief Get the file descriptor
   * @return int The underlying file descriptor or -1 if invalid
   */
  int fd() const { return fd_; }

  /**
   * @brief Get the size of the file
   * @return off_t Size of the file in bytes
   */
  off_t size() const { return size_; }
};

/**
 * @brief Sends an HTTP error response to the client
 *
 * @param client_fd Client socket file descriptor
 * @param status_code HTTP status code
 * @param status_text HTTP status text
 * @param headers Additional headers to include (can be empty)
 * @param body Response body (can be empty)
 */
void send_http_response(int client_fd, int status_code,
                        const std::string &status_text,
                        const std::string &headers, const std::string &body) {
  std::string response =
      "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n";
  if (!headers.empty()) {
    response += headers;
  }
  if (!body.empty()) {
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }
  response += "Connection: close\r\n";
  response += "\r\n";
  response += body;

  ssize_t sent =
      send(client_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
  if (sent < 0 && errno != EPIPE) {
    perror("Failed to send HTTP response");
  }
}

/**
 * @brief Sends a file to the client using zero-copy sendfile
 *
 * @param client_fd Client socket file descriptor
 * @param file File object to send
 * @return true if successful, false on error
 */
bool send_file_content(int client_fd, const File &file) {
  off_t offset = 0;
  off_t remaining = file.size();
  const off_t chunk_size = config::SEND_CHUNK_SIZE;

  while (remaining > 0) {
    ssize_t sent = sendfile(client_fd, file.fd(), &offset,
                            std::min(remaining, chunk_size));

    if (sent <= 0) {
      if (errno == EAGAIN || errno == EINTR) {
        continue; // Retry on temporary errors
      }
      if (errno != EPIPE) { // Ignore broken pipe
        perror("sendfile() failed");
        return false;
      }
      break;
    }
    remaining -= sent;
  }
  return true;
}

/**
 * @brief Handles a client connection in a separate thread
 *
 * This function is executed in a separate thread for each client connection.
 * It processes the HTTP request and sends the appropriate response.
 *
 * @param arg Pointer to the client file descriptor (int)
 * @return void* Always returns NULL
 */
void *handle_client(void *arg) {
  int client_fd = *(int *)arg;
  free(arg); // Free the allocated memory

  try {
    // Read client request (first 4KB should be enough for headers)
    char buffer[4096];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
      close(client_fd);
      return NULL;
    }
    buffer[bytes_read] = '\0';

    // Parse request line
    std::string_view request(buffer);

    // Check for GET or HEAD method
    bool is_head = (request.find("HEAD ") == 0);
    if (!is_head && request.find("GET ") != 0) {
      // Method not allowed
      std::string allow_header = "Allow: GET, HEAD\r\n";
      send_http_response(client_fd, 405, "Method Not Allowed",
                         "Content-Type: text/plain\r\n" + allow_header,
                         "405 Method Not Allowed\n");
      shutdown(client_fd, SHUT_RDWR);
      close(client_fd);
      return NULL;
    }

    // Open file to send
    File file(config::FILE_PATH.data());

    // Build and send headers
    std::string headers =
        "Content-Length: " + std::to_string(file.size()) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";

    send_http_response(client_fd, 200, "OK", headers, "");

    // For HEAD requests, we don't send the body
    if (!is_head) {
      send_file_content(client_fd, file);
    }
  } catch (const std::exception &e) {
    send_http_response(client_fd, 500, "Internal Server Error",
                       "Content-Type: text/plain\r\n",
                       "500 Internal Server Error\n");
  }

  shutdown(client_fd, SHUT_RDWR);
  close(client_fd);
  return NULL;
}

/**
 * @brief Main entry point of the server
 *
 * Sets up signal handling, initializes the server socket, and enters the main
 * accept loop to handle incoming client connections.
 *
 * @return int Exit status (0 on success, non-zero on error)
 */
int main() {
  // Ignore SIGPIPE to prevent server from exiting when writing to a closed
  // socket This allows us to handle broken pipe errors gracefully in our code
  signal(SIGPIPE, SIG_IGN);

  try {
    // Open the file at startup to verify it exists and cache its size
    // This also serves as a quick check that we can access the file before
    // accepting connections
    File file(config::FILE_PATH.data());

    // Set up server socket
    Socket server_socket = create_server_socket();
    printf("Server running. Press Ctrl+C to exit...\n");

    // Main server loop: accept connections and handle them in separate threads
    while (true) {
      printf("Waiting for connection...\n");

      // Accept a new client connection
      // This is a blocking call that will wait until a client connects
      ClientInfo client = server_socket.accept();
      printf("Accepted connection from %s:%d\n", client.ip.c_str(),
             client.port);

      // Use unique_ptr for automatic cleanup of client FD in case of errors
      // This ensures the FD is closed if thread creation fails
      auto client_fd = std::make_unique<int>(client.fd);
      if (!client_fd) {
        // If allocation fails, we need to close the socket manually
        close(client.fd);
        handle_error("Failed to allocate client FD");
      }

      // TODO: In a production environment, consider using a thread pool
      // to limit the number of concurrent connections and prevent resource
      // exhaustion
      pthread_t thread;
      // Create a new thread to handle the client connection
      // The client_fd is released to the thread (ownership transfer)
      if (pthread_create(&thread, NULL, handle_client, client_fd.release()) !=
          0) {
        // If thread creation fails, clean up the client socket
        close(client.fd);
        handle_error("Failed to create client thread");
      }

      // Detach the thread so its resources are automatically released when it
      // exits This is preferred over joining since we don't need to synchronize
      // with the thread
      if (pthread_detach(thread) != 0) {
        // If detach fails, log a warning but continue
        // The thread will still run and clean up after itself when it exits
        perror("Warning: pthread_detach() failed");
      }

      printf("Serving %ld bytes to %s:%d\n", file.size(), client.ip.c_str(),
             client.port);
    }
  } catch (const std::exception &e) {
    handle_error(e.what());
  }

  return 0;
}
