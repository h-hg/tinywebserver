#ifndef TIMER_H_
#define TIMER_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

class Timer {
 public:
  using duration = std::chrono::microseconds;
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

 protected:
  struct Task {
    /**
     * @brief The unique id of the task.
     */
    size_t id;

    std::function<void()> call_back;

    /**
     * @brief The delay of task after the timer starts.
     */
    duration start_delay;

    /**
     * @brief The number of repeat runs. -1 stands infinite, 0 means unvalid
     */
    int times;

    /**
     * @brief The interval between runs.
     */
    duration interval;

    /**
     * @brief The next run time of the task.
     */
    time_point next_run_time;

    Task(size_t id, std::function<void()> &&call_back, duration start_delay,
         int times = 1, duration interval = duration::zero())
        : id(id),
          call_back(std::move(call_back)),
          start_delay(start_delay),
          times(times),
          interval(interval) {}

    bool is_valid() const { return times != 0; }

    void cancel() { times = 0; }

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

 public:
  /**
   * @brief Add task. The task won't run until start() is call. Each function
   * will be run after its specified start_delay. Functions may also be added
   * after start() has been called, in which case start_delay is still honored.
   * It will throw exception if there is some invalid parameters.
   * @return The unique task id.
   */
  size_t add(std::function<void()> &&call_back, duration start_delay,
             int times = 1, duration interval = duration::zero());

  /**
   * @brief Remove the tasks by id.
   * @return Return false if no such task exist.
   */
  bool cancel(size_t id);

  /**
   * @brief Start a backstage thread to schedule the tasks.
   * @return Return false if the timer is running.
   */
  bool start();

  /**
   * @brief Stop the timer temporarily. It may be restarted later by calling
   * start() again.
   * @return Return false if the timer has been stopped.
   */
  bool stop();

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
  void clear();

  Timer() = default;

  ~Timer() { stop(); }

 protected:
  /**
   * @brief A struct to com
   */
  struct PTaskOrder {
    bool operator()(const std::unique_ptr<Task> &lhs,
                    const std::unique_ptr<Task> &rhs) {
      return lhs->next_run_time > rhs->next_run_time;
    }
  };

  /**
   * @brief A backstage function to schedual the task.
   */
  void worker();

  /**
   * @brief Run one task from tasks_. The lock should be locked before calling.
   */
  void run_one_task(std::unique_lock<std::mutex> &lock, clock::time_point now);

  /**
   * @brief The next id of the task
   */
  std::atomic<size_t> next_id_ = 0;

  /**
   * @brief A atomic variable to indicate whether timer is running.
   */
  std::atomic<bool> running_{false};

  /**
   * @brief Condition variable that is signalled whenever a new function is
   * added or when the FunctionScheduler is stopped.
   */
  std::condition_variable running_cv_;

  /**
   * @brief The thread that schedule tasks.
   */
  std::thread thread_;

  /**
   * @brief mutex for tasks_
   */
  std::mutex tasks_mutex_;

  /**
   * @brief Tasks container. It uses a minimal heap to manage elements. To avoid
   * memory thrashing, it may contain some invalid task. And The currently
   * scheduling task will be popped up from it temporarily, because the
   * next_run_time_ of task needs to be recalculated.
   * @note The reason why std::priority_queue is not used is that the start()
   * function needs to readjust all the next_run_time_ of tasks, and why
   * std::priority_queue does not provide such interface to operate the elements
   * inside.
   */
  std::vector<std::unique_ptr<Task>> tasks_;

  /**
   * @brief Task id to Task object
   * @note The id will be removed from id2tasks_ if and only if the task is
   * deleted. Different from tasks_, there is no invalid task in it and the
   * currenting scheduling task won't be popped temporarily.
   */
  std::unordered_map<size_t, Task *> id2tasks_;

  /**
   * @brief To sort the task in tasks_
   */
  PTaskOrder cmp_;

  /**
   * @brief Condition variable that is signalled whenever a new function is
   * added or when the FunctionScheduler is stopped.
   */
  std::condition_variable running_cv_;

  std::atomic<bool> steady_ = false;
};

#endif