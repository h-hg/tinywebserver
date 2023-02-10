// todo

#ifndef HTTP_SERVER_
#define HTTP_SERVER_

#include <cstdint>
#include <string>

#include "tinywebserver/network/epoller.h"
#include "tinywebserver/network/http/request.h"
#include "tinywebserver/network/http/response_writer.h"
#include "tinywebserver/pool/thread_pool.hpp"

namespace http {

class Server {
 public:
  using HttpHandler = std::function<void(ResponseWriter &, const Request &)>;

 protected:
  class Connection {
   public:
    /**
     * @param fd File descriptor of client socket
     * @param is_et Whether fd is in the edge triger mode.
     */
    Connection(int fd, sockaddr_in addr, bool is_et = true)
        : fd_(fd), addr_(addr), is_et_(is_et) {}

    /**
     * @brief Write HTTP Reponse to the client fd
     */
    void write();

    void close();

   protected:
    /**
     * @brief fd of client
     */
    int fd_;

    bool is_et_;

    /**
     * @brief Address of client
     */
    sockaddr_in addr_;

    ResponseWriter resp_writer_;
    Request req_;
  };

 public:
  bool register_handler(const std::string &prefix, HttpHandler &&handler);

  bool listen(const std::string address, uint16_t port);

  bool start();

  /**
   * @brief Set the triger mode of listen fd and client fd.
   * @param is_listen_et Whether listen fd uses edge triger
   * @param is_client_et Whether client fd uses edge triger
   */
  void set_triger_mode(bool is_listen_et = true, bool is_client_et = true);

 protected:
  void acceptor();

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

  epoll_event event;
};

}  // namespace http

#endif