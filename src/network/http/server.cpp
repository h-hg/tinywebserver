// todo

#include "tinywebserver/network/http/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>

#include "tinywebserver/network/linux_wrapper.h"

namespace http {

void Server::set_triger_mode(bool is_listen_et = true,
                             bool is_client_et = true) {
  listen_fd_event_ = EPOLLRDHUP;
  client_event_ = EPOLLONESHOT | EPOLLRDHUP;
  if (is_listen_et) listen_fd_event_ |= EPOLLET;
  if (is_client_et) client_event_ |= EPOLLET;
}

bool Server::listen(uint16_t port, const std::string address) {
  if (running_ || port < 1024) return false;

  if (listen_fd_ != -1) {
    ::close(listen_fd_);
    // todo: clear all the data, such as epoller, connections
  }

  // create the socket
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;

  // todo struct linger

  /* 端口复用 */
  /* 只有最后一个套接字会正常接收数据。 */
  if (int optval = 1; setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                                 (const void *)&optval, sizeof(int)) == -1) {
    // std::cout<<"set socket setsockopt error !"<<std::endl;
    close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  // bind
  sockaddr_in serv_addr;
  serv_addr.sin_port = htons(port);
  memset(&serv_addr, 0, sizeof(serv_addr));
  if (address.empty())
    serv_addr.sin_addr.s_addr = INADDR_ANY;
  else if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr.s_addr) <=
           0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&serv_addr),
           sizeof(serv_addr)) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  // listen
  // todo backlog
  if (::listen(listen_fd_, 6) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  // add to epoll tree
  epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = listen_fd_event_;
  event.data.fd = listen_fd_;
  if (epoller_.add(listen_fd_, event, nullptr) == false) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  set_fd_nonblock(listen_fd_);

  return true;
}

bool Server::start() {
  if (listen_fd_ == -1 || running_) return false;

  while (running_) {
    int error = 0;
    int n = epoller_.wait(-1, &error);
    if (n == -1 && ((error == ECONNABORTED || error == EINTR))) continue;
    for (int i = 0; i < n; ++i) {
      auto event = epoller_[i];
      int fd = event.data.fd;
      if (fd == listen_fd_) {
        acceptor();
      } else if (event.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        // close fd
      } else if (event.events & EPOLLIN) {
        // readable
      } else if (event.events & EPOLLOUT) {
        // writeable
      } else {
        // log unknown event
      }
    }
  }
  // todo
  return true;
}

void Server::acceptor() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listen_fd_, (struct sockaddr *)&addr, &len);
    if (fd <= 0) return;
    // todo
  } while (listen_fd_event_ & EPOLLET);
}

void Server::on_read(int fd) {
  // todo: update expire time
  auto it = this->client_fd_to_con_.find(fd);
  if (it == client_fd_to_con_.end()) {
    // todo close fd;
    return;
  }
  auto &con = it->second;

  threadpool_.push_task([this, &con]() {
    do {
      int error = 0;
      auto state = 0;
    } while ()
  });
}

}  // namespace http
