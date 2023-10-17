#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <error.h>
#include <cerrno>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <vector>
#include <algorithm>


std::vector<int> online_clients;

in_addr_t server_ip;
in_port_t server_port;
int socket_fd;
int epoll_fd;
struct epoll_event events[SOMAXCONN];

void close_socket(int socket) {
  shutdown(socket, SHUT_RDWR);
  close(socket);
}

int unlock_io(int fd) {
  int flags = fcntl(fd, F_GETFL);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int register_read_interest(int _epoll_fd, int fd) {
  struct epoll_event ready_for_reading{};
  ready_for_reading.events = EPOLLIN;
  ready_for_reading.data.fd = fd;
  return epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ready_for_reading);
}

int init_epoll(int _socket_fd) {
  unlock_io(_socket_fd);
  int _epoll_fd = epoll_create1(0);
  if (-1 == _epoll_fd) {
    error(
            EXIT_FAILURE,
            errno,
            "could not create epoll instance");
  }
  register_read_interest(_epoll_fd, _socket_fd);
  return _epoll_fd;
}

int create_server(const in_addr_t _server_ip, const in_port_t _server_port) {
  int _socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = _server_ip;
  server_addr.sin_port = _server_port;
  if (bind(_socket_fd, (const struct sockaddr *) &server_addr,
           sizeof(struct sockaddr_in)) == -1) {
    close(_socket_fd);
    error(
            EXIT_FAILURE,
            errno,
            "Could not bind"
    );
  }
  if (listen(_socket_fd, SOMAXCONN) == -1) {
    close(_socket_fd);
    error(
            EXIT_FAILURE,
            errno,
            "Could not start to listen"
    );
  }
  return _socket_fd;
}

void accept_client() {
  int client_fd;
  while ((client_fd = accept(socket_fd, nullptr, nullptr)) != -1) {
    unlock_io(client_fd);
    register_read_interest(epoll_fd, client_fd);
    online_clients.push_back(client_fd);
  }
}

void process_fd(int fd) {
  if (fd == socket_fd) {
    accept_client();
    return;
  }
  const int buf_size = 1024;
  char received[buf_size];

  if (read(fd, received, buf_size) <= 0) {
//    close_socket(fd);
    return;
  }

  if (strcmp("0", received) == 0) {
    online_clients.erase(
            std::remove(online_clients.begin(), online_clients.end(), fd),
            online_clients.end()
    );
    return;
  }

  if (strcmp("1", received) == 0) {
    printf("client %d got message\n", fd);
    return;
  }

  printf("client %d: %s\n\n", fd, received);
}

void send_message(char *msg, int fd) {
  if (send(fd, msg, strlen(msg), 0) <= 0) {
//    close_socket(fd);
    return;
  }
}

void broadcast_message(char *msg) {
  char broadcast_tag[100] = "[broadcast] ";
  for (int online_client: online_clients) {
    send_message(strcat(broadcast_tag, msg), online_client);
  }
}

void *client_processing(void *data) {
  while (true) {
    int read_ready_count = epoll_wait(epoll_fd, events, SOMAXCONN, -1);
    if (read_ready_count == -1) {
      break;
    }
    for (size_t i = 0; i < read_ready_count; ++i) {
      process_fd(events[i].data.fd);
    }
  }
  return nullptr;
}

void handle_print() {
  if (online_clients.empty()) {
    printf("no users");
  } else {
    printf("list of users' file descriptors: [");
    for (int online_client: online_clients) {
      printf("%d, ", online_client);
    }
    printf("]");
  }
  printf("\n\n");
}

void handle_broadcast() {
  char message[1024];
  printf("write your message: ");
  scanf("%s", message);
  broadcast_message(message);
  printf("\n");
}

void handle_message() {
  char message[1024];
  int fd;
  printf("write your message: ");
  scanf("%s", message);
  printf("choose client: ");
  scanf("%d", &fd);
  send_message(message, fd);
  printf("\n");
}

void *server(void *data) {
  char cmd[128];

  while (true) {
    scanf("%s", cmd);
    if (strcmp(cmd, "print") == 0) {
      handle_print();
    }
    if (strcmp(cmd, "broadcast") == 0) {
      handle_broadcast();
    }
    if (strcmp(cmd, "message") == 0) {
      handle_message();
    }
  }
}

int main(int argc, char **argv) {

  server_ip = inet_addr("127.0.0.1");
  server_port = htons(strtol(argv[1], nullptr, 10));

  socket_fd = create_server(server_ip, server_port);
  epoll_fd = init_epoll(socket_fd);

  pthread_t th1, th2;
  pthread_create(&th1, nullptr, client_processing, nullptr);
  pthread_create(&th2, nullptr, server, nullptr);
  pthread_join(th1, nullptr);
  pthread_join(th2, nullptr);

  close(epoll_fd);
  close(socket_fd);
  return 0;
}