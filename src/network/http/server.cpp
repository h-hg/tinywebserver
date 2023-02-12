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
  if (epoller_.add(listen_fd_, listen_fd_event_ | EPOLLIN) == false) {
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
        close(fd);
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

void Server::acceptor() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listen_fd_, (struct sockaddr *)&addr, &len);
    if (fd <= 0) return;
    // todo
  } while (listen_fd_event_ & EPOLLET);
}

bool Server::close(int fd) {
  if (client_fd_to_conn_.count(fd)) {  // 服务器端关闭连接
    client_fd_to_conn_[fd].close();
    client_fd_to_conn_.erase(fd);
  }
}

void Server::on_read(int fd) {
  // todo: update expire time
  auto it = this->client_fd_to_conn_.find(fd);
  if (it == client_fd_to_conn_.end()) {
    // todo close fd;
    return;
  }
  auto &conn = it->second;

  threadpool_.push_task([this, &conn]() {
    // do {
    //   int error = 0;
    //   auto state = 0;
    // } while (conn.is_et_ & EPOLLET);

    // read data into conn
    int ret = -1, read_errno = 0;
    ret = conn.read(read_errno);
    if (ret < 0 && read_errno != EAGAIN) {
      // no need to response
      this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLIN);
      return;
    }

    conn.parse_request();
    conn.process();
    conn.make_response();

    bool ret = this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLOUT);
    if (!ret) close(conn.fd);
  });
}

void Server::on_write(int fd) {
  auto it = this->client_fd_to_conn_.find(fd);
  if (it == client_fd_to_conn_.end()) {
    // todo close fd;
    return;
  }
  auto conn & = it->second;

  threadpool_.push_task([this, &conn]() {
    int ret = -1, write_errno = 0;
    ret = conn.write(write_errno);
    if (ret < 0) {  // if error occurs when writing to socket fd
      if (write_errno == EAGAIN) {
        this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLOUT);
      } else {
        this->close(conn.fd_);
      }
    } else if (conn.keep_alive_) {
      this->epoller_.mod(conn.fd_, this->client_event_ | EPOLLIN);
    } else {
      this->close(conn.fd_);
    }
  });
}

int Server::Connection::read(int &read_errno) {
  int ret = req_parser_.consume_from_fd(conn.fd_, conn.is_et_);
  if (ret < 0) read_errno = errno;
  return ret;
}

int Server::Connection::write(int &write_errno) {
  // todo
}

bool Server::Connection::parse_request() {
  parse_success_ = false;
  if (!req_parser_.parse_request_line(req_) || !ret =
          req_parser_.parse_header(req_.header)) {
    return false;
  }

  // todo parse request body

  parse_success_ = true;
  return true;
}

bool Server::Connection::process() {
  // todo process request
  keep_alive_ = req_.is_keepalive();
}

bool Server::Connection::make_response() {
  if (!parse_success_) {
    status_ = Response::StatusCode::BAD_REQUEST;
    resp_writer_.set_srcpath("");  // todo: set srcpath to 400.html
  }

  struct stat *mmfile_stat = resp_writer_.mmfilestat();
  if (stat((resp_writer_.srcpath()).data(), mmfile_stat) < 0 ||
      S_ISDIR(mmfile_stat->st_mode)) {
    status_ = Response::StatusCode::NOT_FOUND;
  } else if (!(mmfile_stat->st_mode & S_IROTH)) {
    code_ = Response::StatusCode::FORBIDDEN;
  } else if (code_ == Response::StatusCode::INVALID_CODE) {
    code_ = Response::StatusCode::OK;
  }

  resp_writer_.set_version("HTTP/1.1");

  if (!Response::CodeToStatus.count(status_)) {
    status_ = Response::StatusCode::BAD_REQUEST;
  }
  resp_writer_.set_status(status);
  resp_writer_.set_desc(Response::CodeToStatus.find(status_)->second));

  // todo: set header and map src file to memory
}

}  // namespace http
