#ifndef EPOLLER_H_
#define EPOLLER_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <shared_mutex>
#include <vector>

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

  ~Epoller() { this->close(); }

  void close() {
    ::close(epfd_);
    epfd_ = -1;
  }

  /**
   * @brief Add fd to the epoll tree
   */
  bool add(int fd, epoll_event event) {
    if (fd < 0) return false;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) != 0) return false;
    ++n_fd_;
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
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) != 0) return false;
    --n_fd_;
    return true;
  }

  /**
   * @brief Epoll wait.
   * @note While one thread is blocked in a call to epoll_pwait(), it is
   * possible for another thread to add a file descriptor to the waited-upon
   * epoll instance. If the new file descriptor becomes ready, it will cause the
   * epoll_wait() call to unblock.
   */
  int wait(int timeout = -1) {
    std::shared_lock lock(mutex_);
    return epoll_wait(epfd_, &events_[0], events_.size(), timeout);
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

  int capacity() const {
    std::shared_lock lock(mutex_);
    return events_.size();
  }

  /**
   * @brief Adaptively adjust the size of epoll event buffer according to the
   * number of fd
   */
  void resize() {
    std::lock_guard lock(mutex_);
    if (n_fd_ < events_.size() / 2 && events_.size() > min_cap_)
      // Using int(0.75 * events_.size()) instead of 0.5 * events_.size() to
      // left space for the epoll event to be added.
      events_.resize(std::max(min_cap_, int(0.75 * events_.size())));
    else if (n_fd_ > events_.size())
      // Don't expand to twice its original size since it will cause memory
      // thrashing if n_fd_ decreased again.
      events_.resize(n_fd_ * 1.5);
  }

  /**
   * @brief Resize the epoll event buffer.
   */
  void resize(int size) {
    std::lock_guard lock(mutex_);
    events_.resize(std::max(min_cap_, size));
  }

  void clear() {
    this->close();
    epfd_ = epfd_ = epoll_create(1);
  }

 protected:
  /**
   * @brief Number of fd on the epoll tree.
   */
  std::atomic<int> n_fd_ = 0;

  int min_cap_;

  std::shared_mutex mutex_;

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