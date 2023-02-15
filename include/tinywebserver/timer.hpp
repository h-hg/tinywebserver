#ifndef TIMER_H_
#define TIMER_H_

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

#include "tinywebserver/utils/kvheap.hpp"

/**
 * @brief
 * @tparam ID The unique id of the task.
 */
template <typename ID>
class Timer {
 public:
  using duration = std::chrono::microseconds;
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  class Task {
   public:
    std::function<void()> call_back;

    /**
     * @brief The delay of task after the timer starts.
     */
    duration start_delay;

    /**
     * @brief The number of repeat runs. Negative number stands infinite, 0
     * means unvalid.
     */
    int times;

    /**
     * @brief The interval between runs. This attribute takes effects when times
     * != 0.
     */
    duration interval;

    /**
     * @brief The next run time of the task.
     */
    time_point next_run_time;

    Task(std::function<void()> &&call_back, duration start_delay, int times = 1,
         duration interval = duration::zero())
        : call_back(std::move(call_back)),
          start_delay(start_delay),
          times(times),
          interval(interval) {}

    /**
     * @brief check validity
     */
    static bool check(Task &task) {
      return check(task.call_back, task.start_delay, task.start_delay,
                   task.times, task.interval);
    }

    static bool check(const std::function<void()> &call_back,
                      duration start_delay, int times, duration interval) {
      if (!call_back) return false;
      if (start_delay < duration::zero()) return false;
      if (times == 0) return false;
      if (interval < duration::zero()) return false;
      return true;
    }

    /**
     * @brief Determine whether the task needs to be rescheduled.
     */
    bool need_schedule() const { return times != 0 && call_back != nullptr; }

    /**
     * @brief Cancel the task.
     */
    void cancel() {
      times = 0;
      call_back = {};
    }

    /**
     * @brief reset the next_run_time according to the start_delay and now.
     */
    void reset_next_run_time(time_point now) {
      next_run_time = now + start_delay;
    }

    /**
     * @brief Reduce the scheduling times of task.
     */
    void reduce_times() {
      if (times > 0) --times;
    }
  };

 protected:
  /**
   * @brief To sort the task in tasks_
   */
  inline static const std::function<bool(const std::unique_ptr<Task> &,
                                         const std::unique_ptr<Task> &)>
      cmp_ = [](const std::unique_ptr<Task> &lhs,
                const std::unique_ptr<Task> &rhs) {
        return lhs->next_run_time > rhs->next_run_time;
      };

 public:
  /**
   * @brief Add task. The task won't run until start() is call. Each task
   * will be run after its specified start_delay. Task may also be added
   * after start(), in which case start_delay is still honored.
   × @param task_id Task ID. The task ID can only be reused after
   completing all scheduling times of the task or the task is cancelled..
   * @return Return false if there is some invalid parameters or the task id has
   * been used.
   */
  bool add(ID const &task_id, std::function<void()> &&call_back,
           duration start_delay, int times = 1,
           duration interval = duration::zero()) {
    if (Task::check(call_back, start_delay, times, interval)) return false;

    std::lock_guard lock(tasks_mutex_);

    if (tasks_.count(task_id)) return false;

    // construct a new task
    auto p_task = std::make_unique<Task>(std::move(call_back), start_delay,
                                         times, interval);
    if (running_) {
      p_task->reset_next_run_time(clock::now());
      tasks_.push(task_id, std::move(p_task));
      // Signal the running thread to wake up and see if it needs to change its
      // current scheduling decision.
      running_cv_.notify_one();
    } else {
      tasks_.push(task_id, std::move(p_task));
    }
    return true;
  }

  /**
   * @brief Update Task.
   * @param task_id Task ID
   * @param call_back A call back function to update the Task. If the task is
   * running, it will be executed after running.
   */
  bool update(ID const &task_id, std::function<void(Task &task)> &&call_back) {
    bool ret = false;
    {
      std::lock_guard lock(tasks_mutex_);
      ret = tasks_.update(task_id, call_back);
      if (ret == false && cur_task_ && cur_task_id_ == task_id) {
        // The task you want to update is running
        update_cur_task_ = std::move(call_back);
        ret = true;
      }
    }
    // wait up the worker
    if (running_) running_cv_.notify_one();
    return ret;
  }

  /**
   * @brief Remove the tasks by id.
   * @return Return false if no such task exist.
   */
  bool cancel(size_t task_id) {
    std::lock_guard lock(tasks_mutex_);
    auto ret = tasks_.erase(task_id);
    if (ret == false && cur_task_ && cur_task_id_ == task_id) {
      // The task you want to delete is running
      ret = remove_cur_task_ = true;
    }
    // Note that we don't need to wake up the worker to recalculate wait time
    // even if cancelled task is the next scheduled task, since after
    // waiting for the old time, the worker finds that there is still a period
    // of time for the next task, so it will continue to wait for a new period
    // of time.
    return ret;
  }

  /**
   * @brief Start a backstage thread to schedule the tasks.
   * @return Return false if the timer is running.
   */
  bool start() {
    if (running_) return false;

    std::lock_guard lock(tasks_mutex_);
    // modify the next_run_time
    tasks_.update(
        [now = clock::now()](Task &task) { task.reset_next_run_time(now); });
    running_ = true;
    thread_ = std::thread(&Timer::worker, this);
    return true;
  }

  /**
   * @brief Stop the timer temporarily. It may be restarted later by calling
   * start() again.
   * @return Return false if the timer has been stopped.
   */
  bool stop() {
    if (running_) return false;
    running_ = false;
    // wake the worker
    running_cv_.notify_one();
    thread_.join();
    // clear the flag
    cur_task_ = nullptr;
    remove_cur_task_ = false;
    update_cur_task_ = nullptr;
    return true;
  }

  /**
   * @brief By default steady is false, meaning schedules may lag behind
   * overtime. This could be due to long running tasks or time drift because of
   * randomness in thread wakeup time. By setting steady to true, Timer will
   * attempt to catch up. i.e. more like a cronjob
   */
  void set_steady(bool steady) { steady_ = steady; }

  /**
   * @brief Clear all the tasks
   */
  void clear() {
    std::lock_guard lock(tasks_mutex_);
    tasks_.clear();
    if (cur_task_) {
      remove_cur_task_ = true;
    }
  }

  Timer() : tasks_(cmp_) {}

  ~Timer() { stop(); }

 protected:
  /**
   * @brief A backstage function to schedule the task.
   */
  void worker() {
    std::unique_lock lock(tasks_mutex_);
    while (running_) {
      if (tasks_.empty()) {
        running_cv_.wait(lock);
        continue;
      }
      // Due to the existence of the Timer::update function, the Task may become
      // meaningless
      if (!tasks_.top()->need_schedule()) {
        // remove the task from tasks_
        tasks_.pop();
        continue;
      }

      const auto now = clock::now();
      const auto sleep_time = tasks_.back()->next_run_time - now;
      if (sleep_time < duration::zero()) {
        run_one_task(lock, now);
      } else {
        running_cv_.wait_for(lock, sleep_time);
      }
    }
  }

  /**
   * @brief Run one task from tasks_. The lock should be locked before calling.
   */
  void run_one_task(std::unique_lock<std::mutex> &lock, clock::time_point now) {
    // check the lock
    assert(lock.mutex() == &tasks_mutex_);
    assert(lock.owns_lock());

    // remove from tasks_ and set the flag
    std::tie(cur_task_id_, cur_task_) = tasks_.pop();

    // run the task
    lock.unlock();
    try {
      cur_task_->call_back();
    } catch (...) {
    }
    lock.lock();

    cur_task_->reduce_times();
    // reset the next_run_time_
    if (steady_) {
      // This allows scheduler to catch up
      cur_task_->next_run_time += cur_task_->interval;
    } else {
      // Note that we set nextRunTime based on the current time where we
      // started the function call, rather than the time when the function
      // finishes. This ensures that we call the function once every time
      // interval, as opposed to waiting time interval seconds between calls.
      // (These can be different if the function takes a significant amount of
      // time to run.)
      cur_task_->next_run_time = now + cur_task_->interval;
    }

    if (remove_cur_task_ == false) {
      // update the running task
      if (update_cur_task_) {
        update_cur_task(*cur_task_);
      }
      // Determine whether the task needs to be rescheduled
      if (cur_task_->need_schedule()) {
        tasks_.push(cur_task_id_, std::move(cur_task_));
      }
    }

    // clear the current task state
    cur_task_ = nullptr;
    remove_cur_task_ = false;
    update_cur_task_ = nullptr;
  }

  /**
   * @brief The thread that schedule tasks.
   */
  std::thread thread_;

  /**
   * @brief A atomic variable to indicate whether backstage thread is running.
   */
  std::atomic<bool> running_{false};

  /**
   * @brief Condition variable that is signalled whenever a new function is
   * added / updated or when the Timer is stopped.
   */
  std::condition_variable running_cv_;

  /**
   * @brief mutex for tasks_
   */
  mutable std::mutex tasks_mutex_;

  /**
   * @brief Tasks container. It uses a minimal heap to manage elements. And The
   * currently scheduling task will be popped up from it temporarily, because
   * the next_run_time_ of task needs to be recalculated.
   */
  KVHeap<ID, std::unique_ptr<Task>, decltype(cmp_)> tasks_;

  /**
   * @brief The task id of current running task.
   */
  ID cur_task_id_;

  /**
   * @brief The current running task.
   */
  std::unique_ptr<Task> cur_task_ = nullptr;

  /**
   * @brief A flag to indicate whether to remove current running task after
   * finishing the current running task.
   */
  bool remove_cur_task_ = false;

  /**
   * @brief If update_cur_task_ is not nullptr, the current running task will be
   * updated after executed.
   */
  std::function<void(Task &)> update_cur_task_ = nullptr;

  std::atomic<bool> steady_ = false;
};

#endif
