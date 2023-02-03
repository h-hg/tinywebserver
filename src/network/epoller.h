#ifndef EPOLLER_H_
#define EPOLLER_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <vector>

class Epoller {
 private:
  Epoller(const Epoller&) = delete;
  Epoller& operator=(const Epoller&) = delete;

 public:
  explicit Epoller() { epfd_ = epoll_create(1); }
  ~Epoller() { close(epfd_); }

  /**
   * @brief Add fd to the epoll tree
   */
  bool add(int fd, epoll_event event) {
    if (fd < 0) return false;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) != 0) return false;
    if (++n_fd_ > events_.size()) events_.resize(n_fd_);
    return true;
  }

  /**
   * @brief Modify listening event on the epoll tree
   */
  bool mod(int fd, epoll_event event) {
    if (fd < 0) return false;
    return 0 == epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event);
  }

  /**
   * @brief Remove the fd from the epoll tree
   */
  bool del(int fd) {
    if (fd < 0) return false;
    epoll_event ev = {0};
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev) != 0) return false;
    if (--n_fd_ > events_.size() * 2) events_.resize(n_fd_);
    return true;
  }

  /**
   * @brief Epoll wait
   */
  int wait(int timewait = -1) {
    return epoll_wait(epfd_, &events_[0], static_cast<int>(events_.size()),
                      timewait);
  }

  /**
   * @brief Get the ready event fd by index. Please check the index by yourself.
   */
  const epoll_event& operator[](int i) const { return events_[i]; }

  epoll_event& operator[](int i) { return events_[i]; }

  /**
   * @brief Get the number of fd on the epoll tree.
   */
  int size() const { return n_fd_; }

 private:
  /**
   * @brief Number of fd on the epoll tree.
   */
  int n_fd_;

  /**
   * @brief Descriptor for epoll
   */
  int epfd_;

  /**
   * @brief buffer for storing ready events
   */
  std::vector<epoll_event> events_;
};

#endif