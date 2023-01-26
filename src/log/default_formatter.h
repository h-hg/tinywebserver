#ifndef DEFAULT_FORMATTER_H_
#define DEFAULT_FORMATTER_H_

#include "./log.h"

class DefaultFormatter : public Logger::Formatter {
 public:
  /**
   * @example [level][time][thread_id][location]: content
   */
  virtual std::string operator()(
      Logger::Level level, const std::string &content, std::thread::id id,
      const std::source_location location,
      std::chrono::time_point<std::chrono::system_clock> time) {
    return "[" + format_level(level) + "][" + format_time(time) + "][thread " +
           std::to_string(int(id)) + "]" + "[" + format_location(location) +
           "]: " + content + "\n";
  }

  /**
   * @brief Format the log level
   */
  std::string format_level(Logger::Level level) const {
    switch (level) {
      case Logger::Level::INFO:
        return "INFO";
      case Logger::Level::WRAN:
        return "WRAN";
      case Logger::Level::ERROR:
        return "ERROR";
      case Logger::Level::DEBUG:
        return "DEBUG";
      case Logger::Level::FATAL:
        return "FATAL";
      case Logger::Level::TRACE:
        return "TRACE";
    }
    return "";
  }

  /**
   * @brief Format the time.
   * @example 2023-01-05 09:05:01
   * @todo Using C++ 20 format like https://stackoverflow.com/a/68754043
   */
  std::string format_time(
      std::chrono::time_point<std::chrono::system_clock> time) const {
    // year-month-day hour:min:second
    auto in_time_t = std::chrono::system_clock::to_time_t(time);
    std::string ret(30, '\0');
    std::strftime(&ret[0], ret.size(), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&in_time_t));
    return ret;
  }

  /**
   * @brief Format the location
   * @example main.cpp(23:8) `int main(int, char**)`
   */
  std::string format_location(const std::source_location location) {
    return std::string() + location.file_name() + "(" +
           std::to_string(location.line()) + ":" + location.column() + ") `" +
           location.function_name() + "`";
  }
};

#endif