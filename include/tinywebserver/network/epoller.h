#ifndef EPOLLER_H_
#define EPOLLER_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <vector>

class Epoller {
 private:
  Epoller(const Epoller&) = delete;
  Epoller& operator=(const Epoller&) = delete;

 public:
  explicit Epoller() { epfd_ = epoll_create(1); }
  ~Epoller() { close(epfd_); }

  /**
   * @brief Add fd with events to the epoll tree
   */
  bool add(int fd, uint32_t events, int* p_errno = nullptr) {
    if (fd < 0) return false;
    epoll_event event = {0};
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) != 0) {
      if (p_errno != nullptr) *p_errno = errno;
      return false;
    }
    if (++n_fd_ > events_.size()) events_.resize(n_fd_ + 1);
    return true;
  }

  /**
   * @brief Modify listening event on the epoll tree
   */
  bool mod(int fd, uint32_t events, int* p_errno = nullptr) {
    if (fd < 0) return false;
    epoll_event event = {0};
    event.events = events;
    event.data.fd = fd;
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event) != 0) {
      if (p_errno != nullptr) *p_errno = errno;
      return false;
    }
    return true;
  }

  /**
   * @brief Remove the fd from the epoll tree
   */
  bool del(int fd, int* p_errno = nullptr) {
    if (fd < 0) return false;
    epoll_event ev = {0};
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev) != 0) {
      if (p_errno != nullptr) *p_errno = errno;
      return false;
    }
    if (--n_fd_ > events_.size() * 2) events_.resize(n_fd_ + 1);
    return true;
  }

  /**
   * @brief Epoll wait. It
   * @note While one thread is blocked in a call to epoll_pwait(), it is
   * possible for another thread to add a file descriptor to the waited-upon
   * epoll instance. If the new file descriptor becomes ready, it will cause the
   * epoll_wait() call to unblock.
   */
  int wait(int timeout = -1, int* p_errno = nullptr) {
    auto res = epoll_wait(epfd_, &events_[0], n_fd_, timeout);
    if (res == -1) {
      if (p_errno != nullptr) *p_errno = errno;
      return -1;
    }
    return res;
  }

  /**
   * @brief Get the ready event fd by index. Please check the index by yourself.
   */
  const epoll_event& operator[](int i) const { return events_[i]; }

  epoll_event& operator[](int i) { return events_[i]; }

  const epoll_event& operator[](int i) const { return events_[i]; }

  /**
   * @brief Get the number of fd on the epoll tree.
   */
  int size() const { return n_fd_; }

 private:
  /**
   * @brief Number of fd on the epoll tree.
   */
  std::atomic<int> n_fd_;

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