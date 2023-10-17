#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <error.h>
#include <cerrno>
#include <unistd.h>
#include <cstring>
#include <csignal>

volatile sig_atomic_t st_si_flag = 0;

void st_si_handler(int sig) {
  st_si_flag = 1;
}

void close_socket(int fd) {
  char disconnection_signal[] = "0";
  send(fd, disconnection_signal, strlen(disconnection_signal), 0);
  shutdown(fd, SHUT_RDWR);
  close(fd);
}

int connect_to_server(const char *ip_address, int port_number) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  struct in_addr server_ip{};
  inet_aton(ip_address, &server_ip);
  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr = server_ip;
  server_addr.sin_port = htons(port_number);
  if (0 != connect(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) {
    close_socket(socket_fd);
    error(EXIT_FAILURE, errno, "Could not connect to %s:%d", ip_address, port_number);
  }
  return socket_fd;
}

int main(int argc, char **argv) {

  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGINT);
  sigprocmask(SIG_SETMASK, &mask, nullptr);
  struct sigaction st_si_action{};
  st_si_action.sa_handler = st_si_handler;
  sigaction(SIGINT, &st_si_action, nullptr);

  const char *ip_address = "127.0.0.1";
  int port = atoi(argv[1]);
  int socket_fd = connect_to_server(ip_address, port);
  int buf_size = 1024;
  char received[buf_size];
  char answer[buf_size];
  char ok[] = "1";

  while (st_si_flag != 1) {
    if (0 == recv(socket_fd, received, buf_size, 0)) {
      continue;
    }

    if (st_si_flag != 1) {
      if (send(socket_fd, ok, strlen(ok), 0) <= 0) {
        break;
      }
      printf("%s\n", received);
      printf("want to answer: y/n: ");
      char option[1];
      scanf("%s", option);
      if ((st_si_flag != 1) && (strcmp(option, "y") == 0)) {
        printf("message: ");
        scanf("%s", answer);
        if (send(socket_fd, answer, strlen(answer), 0) <= 0) {
          break;
        }
      }
    }
  }
  close_socket(socket_fd);
  return 0;
}