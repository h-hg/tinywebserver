#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>

class ThreadPool {
 public:
  /**
   * @brief Construct a new thread pool.
   *
   * @param thread_count The number of threads to use. The default value is the
   * total number of hardware threads available, as reported by the
   * implementation. This is usually determined by the number of cores in the
   * CPU. If a core is hyperthreaded, it will count as two threads.
   */
  ThreadPool(unsigned int thread_count)
      : thread_count_(determine_thread_count(thread_count)),
        threads_(std::make_unique<std::thread[]>(
            determine_thread_count(thread_count_))) {
    create_threads();
  }

  /**
   * @brief Destruct the thread pool. Waits for all tasks to complete, then
   * destroys all threads.
   */
  ~ThreadPool() {
    wait_for_tasks();
    destroy_threads();
  }

  /**
   * @brief Wait for tasks to be completed. Normally, this function waits for
   * all tasks, both those that are currently running in the threads and those
   * that are still waiting in the queue. Note: To wait for just one specific
   * task, use submit() instead, and call the wait() member function of the
   * generated future.
   */
  void wait_for_tasks();

  /**
   * @brief Get the number of threads in the pool.
   *
   * @return The number of threads.
   */
  unsigned int get_thread_count() const { return thread_count_; }

  /**
   * @brief Get the number of tasks currently waiting in the queue to be
   * executed by the threads.
   *
   * @return The number of queued tasks.
   */
  size_t get_tasks_queued() const {
    const std::scoped_lock tasks_lock(tasks_mutex_);
    return tasks_.size();
  }

  /**
   * @brief Get the number of tasks currently being executed by the threads.
   *
   * @return The number of running tasks.
   */
  size_t get_tasks_running() const {
    const std::scoped_lock tasks_lock(tasks_mutex_);
    return tasks_total_ - tasks_.size();
  }

  /**
   * @brief Get the total number of unfinished tasks: either still in the queue,
   * or running in a thread. Note that get_tasks_total() == get_tasks_queued() +
   * get_tasks_running().
   *
   * @return The total number of tasks.
   */
  size_t get_tasks_total() const { return tasks_total_; }

  /**
   * @brief Check whether the pool is currently paused.
   *
   * @return true if the pool is paused, false if it is not paused.
   */
  bool is_paused() const { return paused_; }

  /**
   * @brief Pause the pool. The workers will temporarily stop retrieving new
   * tasks out of the queue, although any tasks already executed will keep
   * running until they are finished.
   */
  void pause() { paused_ = true; }

  /**
   * @brief Unpause the pool. The workers will resume retrieving new tasks out
   * of the queue.
   */
  void unpause() { paused_ = false; }

  /**
   * @brief Reset the number of threads in the pool. Waits for all currently
   * running tasks to be completed, then destroys all threads in the pool and
   * creates a new thread pool with the new number of threads. Any tasks that
   * were waiting in the queue before the pool was reset will then be executed
   * by the new threads. If the pool was paused before resetting it, the new
   * pool will be paused as well.
   *
   * @param thread_count The number of threads to use. The default value is the
   * total number of hardware threads available, as reported by the
   * implementation. This is usually determined by the number of cores in the
   * CPU. If a core is hyperthreaded, it will count as two threads.
   */
  void reset(unsigned int thread_count = 0);

  /**
   * @brief Push a function with zero or more arguments, but no return value,
   * into the task queue. Does not return a future, so the user must use
   * wait_for_tasks() or some other method to ensure that the task finishes
   * executing, otherwise bad things will happen.
   *
   * @tparam F The type of the function.
   * @tparam A The types of the arguments.
   * @param task The function to push.
   * @param args The zero or more arguments to pass to the function. Note that
   * if the task is a class member function, the first argument must be a
   * pointer to the object, i.e. &object (or this), followed by the actual
   * arguments.
   */

  template <typename F, typename... A>
  void push_task(F&& task, A&&... args);

  /**
   * @brief Submit a function with zero or more arguments into the task queue.
   * If the function has a return value, get a future for the eventual returned
   * value. If the function has no return value, get an std::future<void> which
   * can be used to wait until the task finishes.
   *
   * @tparam F The type of the function.
   * @tparam A The types of the zero or more arguments to pass to the function.
   * @tparam R The return type of the function (can be void).
   * @param task The function to submit.
   * @param args The zero or more arguments to pass to the function. Note that
   * if the task is a class member function, the first argument must be a
   * pointer to the object, i.e. &object (or this), followed by the actual
   * arguments.
   * @return A future to be used later to wait for the function to finish
   * executing and/or obtain its returned value if it has one.
   */
  template <
      typename F, typename... A,
      typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
  std::future<R> submit(F&& task, A&&... args);

 protected:
  /**
   * @brief Determine how many threads the pool should have, based on the
   * parameter passed to the constructor.
   *
   * @param thread_count The parameter passed to the constructor. If the
   * parameter is a positive number, then the pool will be created with this
   * number of threads. If the parameter is non-positive, or a parameter was not
   * supplied (in which case it will have the default value of 0), then the pool
   * will be created with the total number of hardware threads available, as
   * obtained from std::thread::hardware_concurrency(). If the latter returns a
   * non-positive number for some reason, then the pool will be created with
   * just one thread.
   * @return The number of threads to use for constructing the pool.
   */
  static auto determine_thread_count(unsigned int thread_count)
      -> decltype(thread_count);

  /**
   * @brief Create the threads in the pool and assign a worker to each thread.
   */
  void create_threads();

  /**
   * @brief Destroy the threads in the pool. If there are some tasks still
   * running, it will wait for these tasks.
   */
  void destroy_threads();

  /**
   * @brief A worker function to be assigned to each thread in the pool. Waits
   * until it is notified by push_task() that a task is available, and then
   * retrieves the task from the queue and executes it. Once the task finishes,
   * the worker notifies wait_for_tasks() in case it is waiting.
   */
  void worker();

  /**
   * @brief An atomic variable indicating whether the workers should pause. When
   * set to true, the workers temporarily stop retrieving new tasks out of the
   * queue, although any tasks already executed will keep running until they are
   * finished. When set to false again, the workers resume retrieving tasks.
   */
  std::atomic<bool> paused_ = {false};

  /**
   * @brief An atomic variable indicating to the workers to keep running. When
   * set to false, the workers permanently stop working.
   */
  std::atomic<bool> running_ = {false};

  /**
   * @brief An atomic variable indicating that wait_for_tasks() is active
   * and expects to be notified whenever a task is done.
   */
  std::atomic<bool> waiting_ = {false};

  /**
   * @brief A condition variable used to notify worker() that a new task has
   * become available.
   */
  std::condition_variable task_avail_cv_ = {};

  /**
   * @brief A condition variable used to notify wait_for_tasks() that a
   * tasks is done.
   */
  std::condition_variable task_done_cv_ = {};

  /**
   * @brief A queue of tasks to be executed by the threads.
   */
  std::queue<std::function<void()>> tasks_ = {};

  /**
   * @brief A mutex to synchronize access to the task queue by different
   * threads.
   */
  mutable std::mutex tasks_mutex_ = {};

  /**
   * @brief The number of threads in the pool.
   */
  unsigned int thread_count_ = {0};

  /**
   * @brief An atomic variable to keep track of the total number of unfinished
   * tasks - either still in the queue, or running in a thread.
   */
  std::atomic<size_t> tasks_total_ = {0};

  /**
   * @brief A smart pointer to manage the memory allocated for the threads.
   */
  std::unique_ptr<std::thread[]> threads_ = nullptr;
};

template <typename F, typename... A>
void ThreadPool::push_task(F&& task, A&&... args) {
  std::function<void()> task_function =
      std::bind(std::forward<F>(task), std::forward<A>(args)...);
  {
    const std::scoped_lock tasks_lock(tasks_mutex_);
    tasks_.push(task_function);
  }
  ++tasks_total_;
  task_avail_cv_.notify_one();
}

template <typename F, typename... A, typename R>
std::future<R> ThreadPool::submit(F&& task, A&&... args) {
  std::function<R()> task_function =
      std::bind(std::forward<F>(task), std::forward<A>(args)...);
  auto task_promise = std::make_shared<std::promise<R>>();
  push_task([task_function, task_promise] {
    try {
      if constexpr (std::is_void_v<R>) {
        std::invoke(task_function);
        task_promise->set_value();
      } else {
        task_promise->set_value(std::invoke(task_function));
      }
    } catch (...) {
      try {
        task_promise->set_exception(std::current_exception());
      } catch (...) {
      }
    }
  });
  return task_promise->get_future();
}

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
#endif