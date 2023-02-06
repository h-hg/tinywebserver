#include "./thread_pool.hpp"

auto ThreadPool::determine_thread_count(unsigned int thread_count)
    -> decltype(thread_count) {
  if (thread_count > 0)
    return thread_count;
  else if (auto cnt = std::thread::hardware_concurrency(); cnt > 0)
    return cnt;
  else
    return 1;
}

void ThreadPool::create_threads() {
  running_ = true;
  for (decltype(thread_count_) i = 0; i < thread_count_; ++i)
    threads_[i] = std::thread(&ThreadPool::worker, this);
}

void ThreadPool::wait_for_tasks() {
  waiting_ = true;
  std::unique_lock tasks_lock(tasks_mutex_);
  task_done_cv_.wait(tasks_lock, [this] {
    return (tasks_total_ == (paused_ ? tasks_.size() : 0));
  });
  waiting_ = false;
}

void ThreadPool::destroy_threads() {
  running_ = false;
  task_avail_cv_.notify_all();
  for (decltype(thread_count_) i = 0; i < thread_count_; ++i)
    threads_[i].join();
}

void ThreadPool::reset(unsigned int thread_count) {
  // saving the state
  const bool was_paused = paused_;
  paused_ = true;
  wait_for_tasks();
  destroy_threads();
  thread_count_ = determine_thread_count(thread_count);
  threads_ = std::make_unique<std::thread[]>(thread_count);
  // restore the state
  paused_ = was_paused;
  create_threads();
}

void ThreadPool::worker() {
  while (running_) {
    std::unique_lock tasks_lock(tasks_mutex_);
    task_avail_cv_.wait(tasks_lock,
                        [this] { return !tasks_.empty() || !running_; });
    if (running_ && !paused_) {
      std::function<void()> task = std::move(tasks_.front());
      tasks_.pop();
      tasks_lock.unlock();
      task();
      tasks_lock.lock();
      --tasks_total_;
      if (waiting_) task_done_cv_.notify_one();
    }
  }
}