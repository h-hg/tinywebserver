#ifndef LOG_H_
#define LOG_H_

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <thread>
#include <type_traits>

class Logger {
 protected:
  using WriterPtr =
      std::unique_ptr<std::fstream, std::function<void(std::fstream *)>>;

 private:
  Logger() = default;
  Logger(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger &operator=(Logger &&) = delete;

 public:
  enum class Level {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WRAN = 3,
    ERROR = 4,
    FATAL = 5,
  };

  using Formatter = std::function<std::string(
      Logger::Level, const std::string, std::thread::id,
      const std::source_location,
      std::chrono::time_point<std::chrono::system_clock>)>;

  static const Formatter default_formatter;

  /**
   * @brief Set the writer for the logger. This method will stop the writer
   * thread temporarily.
   */
  bool set(std::unique_ptr<std::fstream> writer);

  /**
   * @brief Set the positive number of logs written by the writer thread at one
   * time. The default value is 8.
   */
  bool set(size_t write_size) {
    if (write_size == 0) return false;
    write_size_ = write_size;
    return true;
  }

  /**
   * @brief Set the log level. The default value is TRACE.
   */
  void set(Level level) { level_ = level; }

  /**
   * @brief Start the writer thread.
   * @return Return false if one of the conditions is met. 1. The writer thread
   * has been started. 2. Writer or Formatter is not set.
   */
  bool start();

  /**
   * @brief Stop the writer thread.
   * @return Return false if the writer thread has not been started.
   */
  bool stop();

  /**
   * @brief Get the instance of the Logger
   */
  static Logger &get_instance() {
    static Logger instance;
    return instance;
  }

  /**
   * @brief Destroy the Logger
   */
  ~Logger();

  /**
   * @brief Log in info level.
   */
  bool info(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::INFO, content, id, location, default_formatter);
  }
  /**
   * @brief Log in warn level.
   */
  bool warn(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::WRAN, content, id, location, default_formatter);
  }

  /**
   * @brief Log in error level.
   */
  bool error(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::ERROR, content, id, location, default_formatter);
  }

  /**
   * @brief Log in debug level.
   */
  bool debug(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::DEBUG, content, id, location, default_formatter);
  }

  /**
   * @brief Log in fatal level.
   */
  bool fatal(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::FATAL, content, id, location, default_formatter);
  }

  /**
   * @brief Log in trace level.
   */
  bool trace(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter) {
    return log(Level::TRACE, content, id, location, default_formatter);
  }

  /**
   * @brief Add log.
   */
  bool log(
      Level level, const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current(),
      const Formatter &formatter = default_formatter);

  /**
   * @brief Flush logs.
   * @return Return false if the writer or formatter hasn't been set.
   */
  bool flush();

 protected:
  Level level_ = Level::TRACE;

  void writer_worker();

  /**
   * @brief mutex for producer
   */
  std::mutex logs_mutex_ = {};

  /**
   * @brief
   */
  std::condition_variable logs_avail_cv_ = {};

  /**
   * @brief buffer of logs
   */
  std::list<std::string> logs_ = {};

  /**
   * @brief writer
   */
  WriterPtr writer_ = nullptr;

  /**
   * @brief writer thread
   */
  std::thread writer_thread_;

  /**
   * @brief The number of logs written by the writer thread at one time.
   */
  std::atomic<size_t> write_size_ = 8;

  /**
   * @brief An atomic variable indicating to the writer to keep running. When
   * set to false, the workers permanently stop working.
   */
  std::atomic<bool> running_ = {false};

  /**
   * @brief An atomic variable indicating that flush().
   */
  std::atomic<bool> waiting_flush_ = {false};

  /**
   * @brief A condition variable used to notify flush().
   */
  std::condition_variable flush_done_cv = {};

  /**
   * @brief Temporary suspension flag, it is used to update the writer
   */
  std::atomic<bool> temporary_stop_ = false;
};

#endif
