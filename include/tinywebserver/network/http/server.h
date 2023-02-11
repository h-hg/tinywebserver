// todo

#ifndef HTTP_SERVER_
#define HTTP_SERVER_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "tinywebserver/network/epoller.h"
#include "tinywebserver/network/http/request_parser.h"
#include "tinywebserver/network/http/response_writer.h"
#include "tinywebserver/pool/thread_pool.hpp"

namespace http {

class Server {
 public:
  using HTTPHandler = std::function<void(ResponseWriter &, const Request &)>;

 protected:
  class Connection {
   public:
    /**
     * @param fd File descriptor of client socket
     * @param is_et Whether fd is in the edge triger mode.
     */
    Connection(int fd = -1, sockaddr_in addr = {}, bool is_et = true)
        : fd_(fd), addr_(addr), is_et_(is_et) {}

    ~Connection() { this->close(); }

    void init();

    /**
     * @brief Write HTTP Reponse to the client fd
     */
    void write();

    bool close() {
      if (fd_ == -1) return false;
      ::close(fd_);
      fd_ = -1;
      return true;
    }

    // protected:
    /**
     * @brief fd of client
     */
    int fd_ = -1;

    bool is_et_ = true;

    /**
     * @brief Address of client
     */
    sockaddr_in addr_;

    ResponseWriter resp_writer_;
    RequestParser req_parser_;
  };

 public:
  ~Server() {
    if (listen_fd_ != -1) close(listen_fd_);
  }

  bool register_handler(const std::string &prefix, HTTPHandler &&handler) {
    prefix_to_handler_[prefix] = std::move(handler);
    return true;
  }

  bool listen(uint16_t port, const std::string address);

  bool start();

  bool stop() {
    if (running_ == false) return false;
    running_ = true;
    return true;
  }

  /**
   * @brief Set the triger mode of listen fd and client fd.
   * @param is_listen_et Whether listen fd uses edge triger
   * @param is_client_et Whether client fd uses edge triger
   */
  void set_triger_mode(bool is_listen_et = true, bool is_client_et = true);

 protected:
  void acceptor();

  void on_read(int fd);

  void on_read(int fd);

  /**
   * @brief Listening file descriptor
   */
  int listen_fd_ = -1;

  Epoller epoller_;

  /**
   * @brief The listening event of listen_fd_
   */
  uint32_t listen_fd_event_ = EPOLLRDHUP;

  /**
   * @brief The listening event of client fd
   */
  uint32_t client_event_ = EPOLLONESHOT | EPOLLRDHUP;

  std::atomic<bool> running_ = {false};

  /**
   * @brief Client file descriptor to Connection.
   */
  std::unordered_map<int, Connection> client_fd_to_con_;

  /**
   * @brief HTTP request URI prefix to HTTPHandler
   */
  std::unordered_map<std::string, HTTPHandler> prefix_to_handler_;

  /**
   * @brief Thread pool
   */
  ThreadPool threadpool_;
};

}  // namespace http

#endif