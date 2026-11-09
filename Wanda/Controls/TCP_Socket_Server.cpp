// TCP Socket headers
#include <sys/socket.h> // Create sockets, accept, send, receive
#include <netinet/in.h> // Network address structures
#include <arpa/inet.h> // Convert IP addresses
#include <unistd.h> // Close sockets

// time logging and storage header
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <vector>
#include <cerrno>

#include <map> // Look up worker names using their IP addresses.
#include <chrono> // Measure the receive waiting limit.
#include <poll.h> // Wait for a socket without waiting forever.
#include <fcntl.h>

using namespace std;

void print_log(const string& message) {
  istringstream lines(message);
  string line;

  while (getline(lines, line)) {
    // Get the current time and convert it into the computer's local calendar time.
    time_t now = time(nullptr);
    tm local_time{}; 

    localtime_r(&now, &local_time);

    // display the timestamp and message
    cout 
    << "["
    << put_time(&local_time, "%Y-%m-%d %H:%M:%S")
    << "] "
    << line
    << '\n';
  }
}

// Set up sever to prepare PPC to receive network
class TcpServer {
private:
  int socket_fd = -1; // Identifies the server's socket.

  // Saves the connected clinet sockets
  vector<int> client_sockets;

  vector<string> worker_names;    // name matching each socket.
  vector<string> incoming_data;  // unfinished replies for each socket.

  map<string, string> allowed_workers; // IP address -> worker name.

  // Close a connection without changing the other workers' indexes.
  void disconnect_worker(size_t worker_index) {
    if (worker_index < client_sockets.size() &&
      client_sockets[worker_index] >= 0) {
      close(client_sockets[worker_index]);
      client_sockets[worker_index] = -1;
      incoming_data[worker_index].clear(); // Discard any unfinished reply.
    }
}

public:
  TcpServer() {
    // AF_INET = IPv4 addresses, SOCK_STREAM = socket type for TCP 
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Verify if the socket was successfully created
    if (socket_fd < 0) {
      print_log("Error: Failed to create socket.");
      return; // Stop setup if creating the socket failed.
    } 

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        print_log("Error: setsockopt(SO_REUSEADDR) failed.");
        close(socket_fd);
        socket_fd = -1;
        return;
        }


    sockaddr_in address{}; // an empty IPv4 address record
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Allow listening on PPC
    address.sin_port = htons(9600);

    // bind(): assigns addresses and port to the socket
    if (::bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
      print_log("Error: Failed to bind socket to port 9600.");
      close(socket_fd);
      socket_fd = -1;
      return;
    }
    print_log("Socket successfully bound to port 9600.");

    // make it a listening socket: actively waiting for a  connection request from a remote client 
    if (listen(socket_fd, 5) < 0) {
      print_log("Error: Failed to listen.");
      close(socket_fd);
      socket_fd = -1; // this shows after closing, socket is no longer open
      return;
    }
    print_log("Server is listening on port 9600.");

    
  }

  // Let controller code check whether setup succeeded.
  bool is_ready() const {
    return socket_fd >= 0;
  }

  // Find a connected worker by name; -1 means unavailable.
  int find_worker(const string& name) const {
    for (size_t i = 0; i < worker_names.size(); ++i) {
      if (worker_names[i] == name && client_sockets[i] >= 0) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  // Tell the server which worker belongs to an IP address.
  void register_worker(const string& name, const string& ip) {
    allowed_workers[ip] = name;
  }
  
  // Accept one registered worker, waiting up to five seconds.
  bool accept_worker() {
    if (!is_ready()) {
        print_log("Error: Server is not ready.");
        return false;
    }

    // Wait up to 5000 milliseconds for a connection request.
    pollfd waiting{};
    waiting.fd = socket_fd;
    waiting.events = POLLIN;

    int ready = ::poll(&waiting, 1, 5000);

    if (ready <= 0 || !(waiting.revents & POLLIN)) {
      print_log("No connection accepted: timeout or socket error.");
      return false;
    }

    // Store the connecting client's address.
    sockaddr_in client_address{};
    socklen_t address_size = sizeof(client_address);

    // Wait for a connection and get its new socket.
    int client_fd = ::accept(
      socket_fd,
      reinterpret_cast<sockaddr*>(&client_address),
      &address_size
    );

    if (client_fd < 0) {
      print_log("Error: Failed to accept connection.");
      return false;
    }

    // Convert the connecting client's address into readable IP text.
    char ip[INET_ADDRSTRLEN]{};

    if (!::inet_ntop(AF_INET, &client_address.sin_addr, ip, sizeof(ip))) {
      close(client_fd);
      print_log("Error: Could not read client IP.");
      return false;
    }

    // Reject IP addresses that the controller has not registered.
    auto match = allowed_workers.find(ip);

    if (match == allowed_workers.end()) {
      close(client_fd);
      print_log("Error: Unknown worker.");
      return false;
    }

    string name = match->second;

#ifdef SO_NOSIGPIPE
    // macOS uses this option instead of Linux's MSG_NOSIGNAL.
    int no_sigpipe = 1;
    if (::setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE,
                     &no_sigpipe, sizeof(no_sigpipe)) < 0) {
      close(client_fd);
      print_log("Error: Failed to configure safe sending.");
      return false;
    }
#endif

    // Do not accept a second active connection for the same worker.
    for (size_t i = 0; i < worker_names.size(); ++i) {
      if (worker_names[i] == name && client_sockets[i] >= 0) {
        close(client_fd);
        print_log("Error: Worker is already connected.");
        return false;
      }
    }

    // Limit how long an individual send can wait.
    timeval timeout{};
    timeout.tv_sec = 3;

    if (::setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO,
      &timeout, sizeof(timeout)) < 0) {
        close(client_fd);
        print_log("Error: Failed to configure send timeout.");
        return false;
      }

    // Save matching entries; undo partial additions if storage fails.
    size_t old_size = client_sockets.size();

    try {
      client_sockets.push_back(client_fd);
      worker_names.push_back(name);
      incoming_data.emplace_back();
    } catch (...) {
      client_sockets.resize(old_size);
      worker_names.resize(old_size);
      incoming_data.resize(old_size);
      close(client_fd);
      throw;
    }

    print_log("Connected: " + name);
    return true;

  }

  // Send one command to a saved connection.
  bool send_command(size_t worker_index, const string& command) {
    if (worker_index >= client_sockets.size() || client_sockets[worker_index] < 0) {
        print_log("Error: Connection not found.");
        return false;
    }

    int client_fd = client_sockets[worker_index];
    string message = command + ";";
    size_t total_sent = 0;
    int send_flags = 0;
#ifdef MSG_NOSIGNAL
    send_flags = MSG_NOSIGNAL;
#endif

    // Keep sending until the complete message has been sent.
    while (total_sent < message.size()) {
      ssize_t sent = ::send(
        client_fd,
        message.data() + total_sent,
        message.size() - total_sent,
        send_flags
    );

    if (sent < 0 && errno == EINTR) {
      continue;
    }

    if (sent <= 0) {
      print_log("Error: Failed to send command.");
      disconnect_worker(worker_index); // Close the failed connection.
      return false;
    }

    total_sent += static_cast<size_t>(sent);
  }

  print_log("Command sent.");
  return true;
  }
  


  // Receive one complete reply, keeping extra bytes for the next call.
  bool receive_data(size_t worker_index, string& reply) {
    reply.clear();
    if (worker_index >= client_sockets.size() || client_sockets[worker_index] < 0) {
      print_log("Error: Connection is unavailable.");
      return false;
    }

    auto deadline = chrono::steady_clock::now() + chrono::seconds(3);
    string& saved = incoming_data[worker_index];

    while (true) {
      size_t ending = saved.find(';');
      if (ending != string::npos && ending <= 4096) {
        reply = saved.substr(0, ending);
        saved.erase(0, ending + 1);
        return true;
      }
      if (saved.size() > 4096) {
        print_log("Error: Reply is too long.");
        disconnect_worker(worker_index);
        return false;
      }

      // The deadline covers the entire reply, including partial arrivals.
      auto remaining = chrono::duration_cast<chrono::milliseconds>(
          deadline - chrono::steady_clock::now()).count();
      if (remaining <= 0) {
        print_log("Error: Reply timed out.");
        disconnect_worker(worker_index);
        return false;
      }

      pollfd waiting{};
      waiting.fd = client_sockets[worker_index];
      waiting.events = POLLIN;
      int ready = ::poll(&waiting, 1, static_cast<int>(remaining));
      if (ready < 0 && errno == EINTR) {
        continue;
      }
      if (ready <= 0 || !(waiting.revents & (POLLIN | POLLHUP | POLLERR))) {
        print_log("Error: Reply timed out or socket check failed.");
        disconnect_worker(worker_index);
        return false;
      }

      char buffer[1024];
      ssize_t received = ::recv(client_sockets[worker_index], buffer,
                                sizeof(buffer), MSG_DONTWAIT);
      if (received < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
        continue;
      }
      if (received <= 0) {
        print_log("Error: Worker disconnected or receive failed.");
        disconnect_worker(worker_index);
        return false;
      }
      saved.append(buffer, static_cast<size_t>(received));
    }
  }

  // Close all remaining sockets when the server object is destroyed.
  ~TcpServer() {
    for (int client_fd : client_sockets) {
      if (client_fd >= 0) {
        close(client_fd);
      }
    }

    // Close the listening socket too.
    if (socket_fd >= 0) {
      close(socket_fd);
      socket_fd = -1;
    }
  }


  // Prevent two objects from accidentally owning the same socket.
  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;


}; // A class definition ends with a semicolon.

