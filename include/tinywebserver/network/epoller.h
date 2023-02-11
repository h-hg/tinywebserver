#ifndef EPOLLER_H_
#define EPOLLER_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <vector>

#include "tinywebserver/utils/spinlock.h"

/**
 * @brief A simple wrapper of Linux epoll. This class is thread safe.
 * @note We don't need to get the errno in designed method, for errono is thread
 * safe.
 */
class Epoller {
 private:
  Epoller(const Epoller&) = delete;
  Epoller& operator=(const Epoller&) = delete;

 public:
  static const int default_min_capacity = 1024 * 4;

  /**
   * @param min_capacity To avoid memory thrashing, the size of the epoll ready
   * event buffer is at least min_capacity.
   */
  explicit Epoller(int min_capacity = default_min_capacity)
      : min_cap_(min_capacity), events_(min_capacity) {
    epfd_ = epoll_create(1);
  }

  ~Epoller() { close(epfd_); }

  /**
   * @brief Add fd to the epoll tree
   */
  bool add(int fd, epoll_event event) {
    if (fd < 0) return false;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) != 0) return false;
    lock.lock();
    ++n_fd_;
    // Don't expand to twice its original size since it will cause memory
    // thrashing if the size of n_fd changes around min_cap_
    if (n_fd_ > events_.size()) events_.resize(n_fd_ * 1.5);
    lock.unlock();
    return true;
  }

  /**
   * @brief Modify listening event on the epoll tree
   */
  bool mod(int fd, epoll_event event) {
    if (fd < 0) return false;
    return epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event) == 0;
  }

  /**
   * @brief Remove the fd from the epoll tree
   */
  bool del(int fd) {
    if (fd < 0) return false;
    epoll_event ev = {0};
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev) != 0) return false;
    lock.lock();
    --n_fd_;
    if (n_fd_ < events_.size() / 2 && events_.size() > min_cap_)
      // use int(0.75 * events_.size()) instead of n_fd_, for events_ will be
      // extended if a new fd added.
      events_.resize(std::max(min_cap_, int(0.75 * events_.size())));
    lock.unlock();
    return true;
  }

  /**
   * @brief Epoll wait. It
   * @note While one thread is blocked in a call to epoll_pwait(), it is
   * possible for another thread to add a file descriptor to the waited-upon
   * epoll instance. If the new file descriptor becomes ready, it will cause the
   * epoll_wait() call to unblock.
   */
  int wait(int timeout = -1) {
    return epoll_wait(epfd_, &events_[0], n_fd_, timeout);
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

 protected:
  /**
   * @brief Number of fd on the epoll tree.
   */
  int n_fd_ = 0;

  int min_cap_;

  SpinLock lock;

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