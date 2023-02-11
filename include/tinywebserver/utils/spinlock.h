#ifndef SPIN_LOCK_H_
#define SPIN_LOCK_H_
#include <atomic>

class SpinLock {
 public:
  void lock() {
    while (flag.test_and_set(std::memory_order_acquire))
      ;
  }

  void unlock() { flag.clear(std::memory_order_release); }

 protected:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

#endif
