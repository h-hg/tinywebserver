#include "./timer.hpp"

#include <algorithm>
#include <cassert>

bool Timer::start() {
  if (running_) return false;

  std::lock_guard lock(tasks_mutex_);
  // modify the next_run_time
  auto const now = clock::now();
  for (auto &ptask : tasks_) ptask->reset_next_run_time(now);
  std::make_heap(tasks_.begin(), tasks_.end(), cmp_);
  running_ = true;
  thread_ = std::thread(std::bind(worker, this));
  return true;
}

bool Timer::stop() {
  if (running_) return false;
  running_ = false;
  // wake the worker
  running_cv_.notify_one();
  thread_.join();
  return true;
}

size_t Timer::add(std::function<void()> &&call_back, duration start_delay,
                  int times, duration interval) {
  if (!call_back)
    throw std::invalid_argument("Timer: Scheduled function must be set");
  if (start_delay < duration::zero())
    throw std::invalid_argument("Timer: start delay must be non-negative.");
  if (times == 0 || times < -1)
    throw std::invalid_argument("Timer: times must be positive or -1");
  if (interval < duration::zero())
    throw std::invalid_argument("Timer: interval must be non-negative.");

  // generate a unique id for the new task
  size_t id = next_id_++;
  for (; id2tasks_.count(id); id = next_id_++)
    ;

  // construct a new task
  auto task = std::make_unique<Task>(id, std::move(call_back), start_delay,
                                     times, interval);
  std::lock_guard lock(tasks_mutex_);
  id2tasks_[id] = task.get();
  tasks_.emplace_back(std::move(task));

  if (running_) {
    tasks_.back()->reset_next_run_time(clock::now());
    std::push_heap(tasks_.begin(), tasks_.end(), cmp_);
    // Signal the running thread to wake up and see if it needs to change its
    // current scheduling decision.
    running_cv_.notify_one();
  }
  return id;
}

bool Timer::cancel(size_t id) {
  std::lock_guard lock(tasks_mutex_);
  if (auto iter = id2tasks_.find(id); iter != id2tasks_.end()) {
    id2tasks_.erase(iter);
    // This will cause that there are some invalid task in the tasks_.
    iter->second->cancel();
    // Note that we don't need to wake up the worker to recalculate wait time
    // even if cancelled task is the next scheduled task, since after
    // waiting for the old time, the worker finds that there is still a period
    // of time for the next task, so it will continue to wait for a new period
    // of time.
    return true;
  }
  return false;
}

void Timer::worker() {
  std::unique_lock lock(tasks_mutex_);
  while (running_) {
    if (tasks_.empty()) {
      // use tasks_ instead of id2tasks_ to remove invalid task
      running_cv_.wait(lock);
      continue;
    }
    if (!tasks_.back()->is_valid()) {
      // remove the task from tasks_
      std::pop_heap(tasks_.begin(), tasks_.end(), cmp_);
      tasks_.pop_back();
      continue;
    }

    const auto now = clock::now();
    const auto sleep_time = tasks_.back()->next_run_time - now;
    if (sleep_time <= duration::zero()) {
      run_one_task(lock, now);
    } else {
      running_cv_.wait_for(lock, sleep_time);
    }
  }
}

void Timer::run_one_task(std::unique_lock<std::mutex> &lock,
                         clock::time_point now) {
  // check the lock
  assert(lock.mutex() == &tasks_mutex_);
  assert(lock.owns_lock());

  // remove from tasks_
  std::pop_heap(tasks_.begin(), tasks_.end(), cmp_);
  auto ptask = std::move(tasks_.back());
  tasks_.pop_back();

  // reset the next_run_time_
  if (steady_) {
    // This allows scheduler to catch up
    ptask->next_run_time += ptask->interval;
  } else {
    // Note that we set nextRunTime based on the current time where we
    // started the function call, rather than the time when the function
    // finishes. This ensures that we call the function once every time
    // interval, as opposed to waiting time interval seconds between calls.
    // (These can be different if the function takes a significant amount of
    // time to run.)
    ptask->next_run_time = now + ptask->interval;
  }

  // run the task
  ptask->reduce_times();
  lock.unlock();
  try {
    ptask->call_back();
  } catch (...) {
  }
  lock.lock();

  // Determine whether the task needs to be reschedule
  if (ptask->is_valid()) {
    tasks_.push_back(std::move(ptask));
    if (running_) {
      std::push_heap(tasks_.begin(), tasks_.end(), cmp_);
    }
  } else {
    id2tasks_.erase(ptask->id);
  }
}

void Timer::clear() {
  std::lock_guard lock(tasks_mutex_);
  bool state = running_;
  stop();
  id2tasks_.clear();
  tasks_.clear();
  if (state) {
    start();
  }
}