#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

// using constexpr to evaluate at compile time
constexpr int PORT = 8080;
constexpr std::string_view FILE_PATH = "/var/www/big_file";

// Error handling function
[[noreturn]] void handle_error(const std::string &msg) {
  throw std::system_error(errno, std::generic_category(), msg);
}

// Socket class
class Socket {
  int fd_ = -1;

public:
  explicit Socket(int domain, int type, int protocol = 0) {
    fd_ = socket(domain, type, protocol);
    if (fd_ < 0) {
      handle_error("socket creation failed");
    }
  }

  // destructor
  ~Socket() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  // Delete copy constructor and assignment
  // So no duplicates of fd_ happens, because copying of
  // fd would mean 2 sockets and would lead to double close
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // Allow move operations
  Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  // Move assignment operator
  // This is used to transfer ownership of the socket
  // from one object to another
  Socket &operator=(Socket &&other) noexcept {
    if (this != &other) {
      if (fd_ >= 0)
        close(fd_);
      fd_ = other.fd_;
      other.fd_ = -1;
    }

    return *this;
  }

  // Set SO_REUSEADDR option to allow address reuse
  // This is useful for development when you want to restart the server
  // without waiting for the socket to be closed
  void set_reuse_addr(bool on = true) {
    int optval = on ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
        0) {
      handle_error("setsockopt(SO_REUSEADDR) failed");
    }
  }

  // Bind the socket to the address
  // This is where the server will listen for incoming connections
  void bind(const sockaddr *addr, socklen_t addr_len) {
    if (::bind(fd_, addr, addr_len) < 0) {
      handle_error("bind() failed");
    }
  }

  // Start listening for incoming connections
  void listen(int backlog = SOMAXCONN) {
    if (::listen(fd_, backlog) < 0) {
      handle_error("listen() failed");
    }
  }

};

// Create a server socket
// This function creates a socket, binds it to the specified port,
// and starts listening for incoming connections
Socket create_server_socket() {
  // Create socket
  Socket sock(AF_INET, SOCK_STREAM);

  // Allow address reuse
  sock.set_reuse_addr(true);

  // Setup server address
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind the socket to the address
  sock.bind(reinterpret_cast<const sockaddr *>(&server_addr),
            sizeof(server_addr));

  // Start listening for incoming connections
  sock.listen();

  printf("Server listening on port %d\n", PORT);
  return sock;
}

int main() {
  try {
    Socket server_socket = create_server_socket();

    // Just keep server running for now
    printf("Press Ctrl+C to exit...\n");

    while (true) {
      // TODO: Add accept here later
      sleep(1);
    }
  } catch (const std::exception &e) {
    fprintf(stderr, "Error: %s\n", e.what());
    return 1;
  }

  return 0;
}
