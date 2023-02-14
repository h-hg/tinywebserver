// todo

#ifndef HTTP_SERVER_
#define HTTP_SERVER_

#include <sys/socket.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "tinywebserver/network/epoller.h"
#include "tinywebserver/network/http/handler.h"
#include "tinywebserver/network/http/request_parser.h"
#include "tinywebserver/network/http/response_writer.h"
#include "tinywebserver/pool/thread_pool.hpp"
#include "tinywebserver/timer.hpp"

namespace http {

class Server {
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
     * @brief Write HTTP Reponse_writer.buf_ to the client fd
     */
    int write(int &write_errno);

    /**
     * @brief Read HTTP Request from the client fd to buffer_in
     * @return error code, 0 for success, -1 for error
     */
    int read(int &read_errno);

    /*
     * @brief parse buffer_in data into struct header
     */
    bool parse_request();

    /*
     * @brief business logic requested by header
     */
    bool process();

    /*
     * @brief make response according to parse_success and srcpath
     */
    bool make_response();

    /**
     * @brief Close the Server
     */
    bool close() {
      if (fd_ == -1) return false;
      ::close(fd_);
      fd_ = -1;
      return true;
    }

   protected:
    /**
     * @brief fd of client
     */
    int fd_ = -1;

    bool is_et_ = true;

    bool keep_alive_ = true;

    /**
     * @brief Address of client
     */
    sockaddr_in addr_;

    ResponseWriter resp_writer_;
    RequestParser req_parser_;  // save buffer_in and provide parse function
    Request req_;

    int status_ = Response::StatusCode::INVALID_CODE;
    bool parse_success_ = true;
  };

 public:
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
  bool start();

  /**
   * @brief Close client
   */
  bool close_client(int client_fd);

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
  /**
   * @brief Get the HTTP Connection by client fd.
   */
  Connection *get_connection(int client_fd) {
    std::shared_lock lock(clients_mutex_);
    if (auto it = clients_.find(client_fd); it != clients_.end())
      return (it->second).get();
    else
      return nullptr;
  }

  void acceptor();

  /*
   * @brief handle EPOLLIN event
   */
  void on_read(int fd);

  /*
   * @brief handle EPOLLOUT event
   */
  void on_write(int fd);

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
   * @brief File descriptor to HTTP Connection
   */
  std::unordered_map<int, std::unique_ptr<Connection>> clients_;

  /**
   * @brief The mutex for clients_
   */
  std::shared_mutex clients_mutex_;

  /**
   * @brief HTTP Handler Manager
   */
  HandlerManager handler_mgr_;

  /**
   * @todo
   */
  HTTPHandler default_handler_;

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