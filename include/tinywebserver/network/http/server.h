// todo

#ifndef HTTP_SERVER_
#define HTTP_SERVER_

#include <sys/socket.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "tinywebserver/network/epoller.h"
#include "tinywebserver/network/http/connection.h"
#include "tinywebserver/network/http/handler.h"
#include "tinywebserver/network/http/request_parser.h"
#include "tinywebserver/network/http/response_writer.h"
#include "tinywebserver/pool/thread_pool.hpp"
#include "tinywebserver/timer.hpp"

namespace http {

class Server {
 public:
  Server() = default;
  ~Server() {
    if (listen_fd_ != -1) close(listen_fd_);
  }

  /**
   * @brief Register the HTTP handler.
   */
  bool handle(const std::string &prefix, HTTPHandler &&handler) {
    return handler_mgr_.handle(prefix, std::move(handler));
  }

  bool listen(uint16_t port, const std::string address);

  /*
   * @brief main thread loop for waiting for epoller
   */
  void start();

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
  void set_triger_mode(bool is_listen_et = true, bool is_client_et = true) {
    listen_fd_event_ = EPOLLRDHUP;
    client_event_ = EPOLLONESHOT | EPOLLRDHUP;
    if (is_listen_et) listen_fd_event_ |= EPOLLET;
    if (is_client_et) client_event_ |= EPOLLET;
  }

 protected:
  void acceptor();

  void close_client(int client_fd);

  /*
   * @brief handle EPOLLIN event
   */
  void on_read(Connection *conn);

  /*
   * @brief handle EPOLLOUT event
   */
  void on_write(Connection *conn);

  /**
   * @brief Listening file descriptor
   */
  int listen_fd_ = -1;

  /**
   * @brief The operation of Linux epoll api
   */
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
   * @brief HTTP Handler Manager
   */
  HandlerManager handler_mgr_;

  ConnectionManger conn_mgr_;

  /**
   * @brief Thread pool
   */
  ThreadPool threadpool_;

  /**
   * @brief Timer
   */
  Timer<int> timer_;
};

}  // namespace http

#endif