#include <ctime>
#include <sstream>
// #include <format> // format haven't been support

#include "./log.h"

Logger::Formatter default_formatter =
    [](Logger::Level level, const std::string content,
       std::thread::id thread_id, const std::source_location location,
       std::chrono::time_point<std::chrono::system_clock> time) -> std::string {
  // format Logger::Level
  static std::unordered_map<Logger::Level, std::string> level2str = {
      {Logger::Level::INFO, "INFO"},   {Logger::Level::WRAN, "WRAN"},
      {Logger::Level::ERROR, "ERROR"}, {Logger::Level::DEBUG, "DEBUG"},
      {Logger::Level::FATAL, "FATAL"}, {Logger::Level::TRACE, "TRACE"},
  };
  // format time
  // year-month-day hour:min:second, for example 2023-01-05 09:05:01
  // TODO: Using C++ 20 std::format like https://stackoverflow.com/a/68754043
  auto in_time_t = std::chrono::system_clock::to_time_t(time);
  std::string strtime(30, '\0');
  std::strftime(&strtime[0], strtime.size(), "%Y-%m-%d %H:%M:%S",
                std::localtime(&in_time_t));

  std::stringstream ss;
  ss << "[" << level2str[level] << "]"  // Level
     << "[" + strtime << "]"            // Time
     << "[thread " << thread_id << "]"  // Thread
     << "[" << location.file_name() << "(" << location.line() << ":"
     << location.column() << ") `" << location.function_name() << "`"
     << "]: " << content << "\n";
  return ss.str();
};

bool Logger::set(std::unique_ptr<std::fstream> writer) {
  // check the parameter
  if (writer == nullptr) return false;

  if (!running_) {
    writer_ = std::move(writer);
    return true;
  }

  // save logs in the original writer and formatter
  flush();
  temporary_stop_ = true;
  // The reason why stop the writer thread is to avoid writer_ conflict
  stop();
  // set the writer
  writer_ = WriterPtr(writer.release(), [](std::fstream *ptr) {
    if (ptr == nullptr) return;
    ptr->flush();
    ptr->close();
    delete ptr;
  });
  start();
  temporary_stop_ = false;
  return true;
}

bool Logger::start() {
  if (running_ || writer_ == nullptr) return false;
  running_ = true;
  writer_thread_ = std::thread(&Logger::writer_worker, this);
  return true;
}

bool Logger::log(Logger::Level level, const std::string &content,
                 std::thread::id id, const std::source_location location,
                 const Logger::Formatter &formatter) {
  // usd temporary_stop_ == true  to allow logging during temporary suspension
  if (running_ == false && temporary_stop_ == true) return false;
  if (level < level_) return false;
  auto time = std::chrono::system_clock::now();
  const auto msg = formatter == nullptr
                       ? content
                       : formatter(level, content, id, location, time);
  // insert the log
  {
    std::lock_guard lock(logs_mutex_);
    logs_.emplace_back(std::move(msg));
  }
  // wake the writer thread
  if (waiting_flush_ || logs_.size() >= write_size_)
    logs_avail_cv_.notify_one();
  return true;
}

bool Logger::flush() {
  if (!running_) return false;
  waiting_flush_ = true;
  std::unique_lock lock(logs_mutex_);
  flush_done_cv.wait(lock, [this] { return logs_.empty(); });
  waiting_flush_ = false;
  return true;
}

bool Logger::stop() {
  if (!running_) return false;
  running_ = false;
  logs_avail_cv_.notify_one();
  writer_thread_.join();
  return true;
}

void Logger::writer_worker() {
  while (running_) {
    std::unique_lock lock(logs_mutex_);
    logs_avail_cv_.wait(
        lock, [this] { return waiting_flush_ || logs_.size() >= write_size_; });
    if (!running_ || logs_.empty()) continue;
    decltype(logs_) logs = std::move(logs_);
    lock.unlock();
    // write logs
    for (auto &log : logs) (*writer_) << log;
    // lock.lock();
    if (waiting_flush_) {
      writer_->flush();
      flush_done_cv.notify_one();
    }
  }
}

Logger::~Logger() {
  if (running_) {
    flush();
    // ensure the writer thread ends before Logger
    stop();
  } else if (writer_ != nullptr) {
    std::unique_lock lock(logs_mutex_);
    decltype(logs_) logs = std::move(logs_);
    lock.unlock();
    for (auto &log : logs) (*writer_) << log;
    writer_->flush();
  }
};
