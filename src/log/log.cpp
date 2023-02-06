#include "./log.h"

bool Logger::set(std::unique_ptr<std::fstream> writer, size_t buf_cell_size,
                 std::unique_ptr<Formatter> formatter) {
  // check the parameter
  if (buf_cell_size == 0 || writer == nullptr || formatter == nullptr)
    return false;

  buf_cell_size_ = buf_cell_size;
  if (!running_) {
    writer_ = std::move(writer);
    formatter_ = std::move(formatter);
    return;
  }

  // save logs in the original writer and formatter
  flush();
  {
    std::lock_guard lock(write_mutex_);
    formatter_ = std::move(formatter);
    writer_ = WriterPtr(writer.release(), [](std::fstream *ptr) {
      if (ptr == nullptr) return;
      ptr->flush();
      ptr->close();
      delete ptr;
    });
  }
  return true;
}

bool Logger::start() {
  if (running_ || writer_ == nullptr || formatter_ == nullptr) return false;
  running_ = true;
  writer_thread_ = std::thread(Logger::writer_worker, this);
  return true;
}

bool Logger::log(
    Level level, const std::string &content,
    std::thread::id id = std::this_thread::get_id(),
    const std::source_location location = std::source_location::current()) {
  if (!running_ || level < level_) return false;
  auto time = std::chrono::system_clock::now();
  auto msg = formatter_->operator()(level, content, id, location, time);
  // insert the log
  {
    std::lock_guard lock(logs_mutex_);
    logs_.emplace_back(std::move(msg));
  }
  // wake the writer thread
  if (waiting_flush_ || logs_.size() >= buf_cell_size_)
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
    logs_avail_cv_.wait(lock, [this] {
      return waiting_flush_ || logs_.size() >= buf_cell_size_;
    });
    if (!running_ || logs_.empty()) continue;
    decltype(logs_) logs = std::move(logs_);
    lock.unlock();
    {
      // write logs
      std::lock_guard lock(write_mutex_);
      for (auto &log : logs) (*writer_) << log;
      // lock.lock();
      if (waiting_flush_) {
        writer_->flush();
        flush_done_cv.notify_one();
      }
    }
  }
}

Logger::~Logger() {
  if (running_) {
    flush();
    // ensure the writer thread ends before Logger
    stop();
  } else if (writer_ != nullptr && formatter_ != nullptr) {
    std::unique_lock lock(logs_mutex_);
    decltype(logs_) logs = std::move(logs_);
    lock.unlock();
    {
      std::lock_guard lock(write_mutex_);
      for (auto &log : logs) (*writer_) << log;
      writer_->flush();
    }
  }
};