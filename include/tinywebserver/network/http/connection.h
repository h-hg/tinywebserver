#ifndef HTTP_CONNECTION_H_
#define HTTP_CONNECTION_H_

#include <netinet/in.h>

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "tinywebserver/network/http/request_parser.h"
#include "tinywebserver/network/http/response_writer.h"

namespace http {

class Connection {
 public:
  /**
   * @param fd File descriptor of client socket

   */
  Connection(int fd = -1, sockaddr_in addr = {}) : fd_(fd), addr_(addr) {}

  ~Connection() { this->close(); }

  int fd() const { return fd_; }

  /**
   * @brief Parsing HTPP Request from file descriptor
   * @param is_et Whether fd is in the edge triger mode.
   */
  auto parse_request_from_fd(bool is_et) {
    if (req_parser_ == nullptr) req_parser_ = std::make_unique<RequestParser>();

    auto p = req_parser_->consume_from_fd(fd_, is_et);
    if (p.second != nullptr) {
      keep_alive_ = p.second->is_keepalive();
    }
    return p;
  }

  ResponseWriter &response_writer() {
    if (req_parser_ == nullptr) req_parser_ = std::make_unique<RequestParser>();
    return *resp_writer_;
  }

  bool is_keep_alive() const { return keep_alive_; }

  /**
   * @brief Get the address of client
   */
  auto address() const -> auto{ return addr_; }

  /*
   * @brief make response according to parse_success and srcpath
   */
  IOVector make_response() {
    full_resp_ = std::make_unique<BufferVector>();
    // todo response line
    full_resp_->write(std::string(resp_writer_->header()));
    full_resp_->write(resp_writer_->buf_);
    resp_ = full_resp_->get_read_iovec();
    return resp_;
  }

  IOVector &response() { return resp_; }

  /**
   * @brief Close the Connection.
   */
  bool close() {
    if (fd_ == -1) return false;
    ::close(fd_);
    fd_ = -1;
    return true;
  }

  void clear() {
    resp_writer_ = nullptr;
    req_parser_ = nullptr;
    full_resp_ = nullptr;
  }

 protected:
  /**
   * @brief fd of client
   */
  int fd_ = -1;

  bool keep_alive_ = true;

  /**
   * @brief Address of client
   */
  sockaddr_in addr_;

  std::unique_ptr<ResponseWriter> resp_writer_ = nullptr;

  std::unique_ptr<RequestParser> req_parser_ = nullptr;

  std::unique_ptr<BufferVector> full_resp_ = nullptr;

  IOVector resp_;
};

class ConnectionManger {
 public:
  ConnectionManger() = default;

  /**
   * @brief Get the HTTP Connection by client fd.
   */
  Connection *get(int fd) const {
    std::shared_lock lock(conn_mutex_);
    if (auto it = conn_.find(fd); it != conn_.end())
      return (it->second).get();
    else
      return nullptr;
  }

  Connection *add(int fd, Connection &&conn) {
    std::lock_guard lock(conn_mutex_);
    auto it = conn_.find(fd);
    if (it == conn_.end()) return nullptr;
    auto sptr = std::make_unique<Connection>(std::move(conn));
    auto ptr = sptr.get();
    conn_.emplace(fd, std::move(sptr));
    return ptr;
  }

  /**
   * @brief Close the HTTP connection
   */
  bool close(int fd) {
    std::lock_guard lock(conn_mutex_);
    auto it = conn_.find(fd);
    if (it == conn_.end()) return false;
    auto &con = *(it->second);
    con.close();
    conn_.erase(it);
    return true;
  }

  void clear() {
    std::lock_guard lock(conn_mutex_);
    for (auto it = conn_.begin(); it != conn_.end(); ++it) it->second->close();
    conn_.clear();
  }

 protected:
  /**
   * @brief File descriptor to HTTP Connection
   */
  std::unordered_map<int, std::unique_ptr<Connection>> conn_;

  /**
   * @brief The mutex for clients_
   */
  mutable std::shared_mutex conn_mutex_;
};
}  // namespace http

#endif