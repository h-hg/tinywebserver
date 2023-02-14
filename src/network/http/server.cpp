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
  epoll_event ev = {.events = listen_fd_event_ | EPOLLIN,
                    .data.fd = listen_fd_};
  // add to epoll tree
  if (epoller_.add(listen_fd_, ev) == false) {
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
    int n = epoller_.wait(-1);
    if (n == -1 && (errno == ECONNABORTED || errno == EINTR)) continue;
    for (int i = 0; i < n; ++i) {
      auto event = epoller_[i];
      int fd = event.data.fd;
      if (fd == listen_fd_) {
        acceptor();
      } else if (event.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        // close fd
        this->close_client(fd);
      } else if (event.events & EPOLLIN) {
        // readable
        on_read(fd);
      } else if (event.events & EPOLLOUT) {
        // writeable
        on_write(fd);
      } else {
        // log unknown event
      }
    }
  }
}

/**
 * finish
 */
void Server::acceptor() {
  // lock the clients_
  std::lock_guard lock(clients_mutex_);

  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listen_fd_, (struct sockaddr *)&addr, &len);
    if (fd <= 0) break;
    clients_.emplace(
        fd, std::make_unique<Connection>(fd, addr, client_event_ & EPOLLET));
  } while (listen_fd_event_ & EPOLLET);
}

/**
 * @brief finish
 */
bool Server::close_client(int client_fd) {
  std::lock_guard lock(clients_mutex_);
  if (auto it = clients_.find(client_fd); it != clients_.end()) {
    auto &con = *(it->second);
    con.close();
    clients_.erase(it);
    epoller_.del(client_fd);
  }
}

/**
 * @todo
 */
void Server::on_read(int client_fd) {
  auto conn_ptr = get_connection(client_fd);
  // todo: update expire time
  if (conn_ptr == nullptr) {
    /**
     * @todo 服务器内部错误，只能把该连接关了
     */
    return;
  }

  // read data from fd
  auto [state, req] = conn_ptr->parse_request_from_fd();
  if (RequestParser::is_error_state(state)) {
    // todo 发送错误原因
    this->close_client(client_fd);
    return;
  }
  if (req == nullptr) {
    epoll_event ev = {.events = this->client_event_ | EPOLLIN,
                      .data.fd = client_fd};
    bool ret = this->epoller_.mod(client_fd, ev);
    if (!ret) {
      // todo 服务器内部错误
      close_client(client_fd);
    }
    return;
  }

  // todo 这里从 request 中获取是否要设置 keep-alive

  // find the http handler
  auto handler = this->handler_mgr_.match(req->uri());
  if (handler == nullptr) {
    // todo 发送找不到 handler 的错误信息
    // 或者尝试使用 default handler
    this->close_client(client_fd);
    return;
  }
  (*handler)(conn_ptr->get_response_writer(), *req);

  conn_ptr->make_response();
  epoll_event ev = {.events = this->client_event_ | EPOLLOUT,
                    .data.fd = client_fd};
  bool ret = this->epoller_.mod(client_fd, ev);
  if (!ret) {
    // 服务器内部错误
    close_client(client_fd);
  }
}

void Server::on_write(int client_fd) {
  auto conn_ptr = get_connection(client_fd);
  // todo: update expire time
  if (conn_ptr == nullptr) {
    /**
     * @todo 服务器内部错误，只能把该连接关了
     */
    return;
  }

  auto &bv = conn_ptr->response();
  // todo 写一个加强版本的 iov 专门负责 更新读写指针。
  auto iov = bv.get_read_iovec();

  int ret = -1, write_errno = 0;
  ret = conn.write(write_errno);
  if (ret < 0) {  // if error occurs when writing to socket fd
    if (write_errno == EAGAIN) {
      this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLOUT);
    } else {
      this->close_client(conn.fd_);
    }
  } else if (conn.keep_alive_) {
    this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLIN);
  } else {
    this->close_client(conn.fd_);
  }
}

}  // namespace http
