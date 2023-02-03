#ifndef LOG_H_
#define LOG_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
// #include <format> // format haven't been support
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <ostream>
#include <source_location>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

class Logger {
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

  class Formatter {
   public:
    virtual std::string operator()(
        Logger::Level level, const std::string &content, std::thread::id id,
        const std::source_location location,
        std::chrono::time_point<std::chrono::system_clock> time) = 0;
  };

  /**
   * @brief Set some essiential parameters
   * @param buf_cell_size The number of logs written by the writer thread at one
   * time.
   * @note This method will block the writer thread.
   */
  void set(size_t buf_cell_size, std::unique_ptr<std::fstream> writer,
           std::unique_ptr<Formatter> formatter);

  /**
   * @brief Set the log level
   */
  void set_level(Level level) { level_ = level; }

  /**
   * @brief Start the writer thread.
   * @return Return false if the writer thread has been started.
   */
  bool start();

  /**
   * @brief Get the instance of the Logger
   */
  static std::shared_ptr<Logger> get_instance() {
    static std::shared_ptr<Logger> ptr(new Logger());
    return ptr;
  }

  /**
   * @brief Destroy the Logger
   */
  ~Logger() {
    destroy_writer_thread();
    flush();
  };

  /**
   * @brief Log in info level.
   */
  bool info(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::INFO, content, id, location);
  }
  /**
   * @brief Log in warn level.
   */
  bool warn(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::WRAN, content, id, location);
  }

  /**
   * @brief Log in error level.
   */
  bool error(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::ERROR, content, id, location);
  }

  /**
   * @brief Log in debug level.
   */
  bool debug(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::DEBUG, content, id, location);
  }

  /**
   * @brief Log in fatal level.
   */
  bool fatal(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::FATAL, content, id, location);
  }

  /**
   * @brief Log in trace level.
   */
  bool trace(
      const std::string &content,
      std::thread::id id = std::this_thread::get_id(),
      const std::source_location location = std::source_location::current()) {
    return log(Level::TRACE, content, id, location);
  }

  /**
   * @brief Add log.
   */
  bool log(Level level, const std::string &content,
           std::thread::id id = std::this_thread::get_id(),
           const std::source_location location = std::source_location::current);

  /**
   * @return Return false if the writer hasn't been set.
   * @note flush is independent to writer thread.
   */
  bool flush();

 protected:
  Level level_ = Level::TRACE;
  /**
   * @brief Write all logs without lock
   */
  void writer_logs(std::list<std::string> &logs) {
    for (auto &log : logs) (*writer_) << log;
  }

  /**
   * @brief destroy writer thread
   */
  void destroy_writer_thread();

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
   * @brief formatter
   */
  std::unique_ptr<Formatter> formatter_ = nullptr;

  /**
   * @brief writer
   */
  std::unique_ptr<std::fstream, std::function<void(std::fstream *)>> writer_ =
      nullptr;

  /**
   * @brief writer thread
   */
  std::unique_ptr<std::thread> writer_thread_ = nullptr;

  /**
   * @brief The number of logs written by the writer thread at one time.
   */
  size_t buf_cell_size_ = 10;

  /**
   * @brief An atomic variable indicating to the writer to keep running. When
   * set to false, the workers permanently stop working.
   */
  std::atomic<bool> running_ = {false};
};

#endif