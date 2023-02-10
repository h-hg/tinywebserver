// todo

#include "tinywebserver/network/http/server.h"

#include <netinet/in.h>
namespace http {

void Server::set_triger_mode(bool is_listen_et = true,
                             bool is_client_et = true) {
  listen_fd_event_ = EPOLLRDHUP;
  client_event_ = EPOLLONESHOT | EPOLLRDHUP;
  if (is_listen_et) listen_fd_event_ |= EPOLLET;
  if (is_client_et) client_event_ |= EPOLLET;
}

bool Server::listen(const std::string address, uint16_t port) {
  if (port < 1024) return false;

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  // todo, using address
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;

  return true;
}

bool Server::start() {
  if (listen_fd_ == -1) return false;
  // todo
  return true;
}

void Server::acceptor() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listen_fd_, (struct sockaddr *)&addr, &len);
    if (fd <= 0) return;
    // else if (HTTPconnection::userCount >= MAX_FD) {
    //   sendError_(fd, "Server busy!");
    //  return;
    // }
    // addClientConnection(fd, addr);
  } while (listen_fd_event_ & EPOLLET);
}

}  // namespace http
