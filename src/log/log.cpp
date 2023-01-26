#include "./log.h"

void Logger::set(size_t buf_cell_size, std::unique_ptr<std::fstream> writer,
                 std::unique_ptr<Formatter> formatter) {
  std::unique_lock lock(logs_mutex_);
  buf_cell_size_ = buf_cell_size;
  if (formatter != nullptr) formatter_ = std::move(formatter);
  if (writer != nullptr) {
    // save logs in the original writer
    decltype(logs_) logs = std::move(logs_);
    // unlike the writer thread, the following writing logs should be locked to
    // avoid appending or writing logs in other threads.
    writer_logs(logs);
    std::unique_ptr<std::fstream, std::function<void(std::fstream *)>>(
        writer.release(), [](std::fstream *ptr) {
          if (ptr == nullptr) return;
          ptr->flush();
          ptr->close();
          delete ptr;
        });
  }
}

bool Logger::start() {
  if (writer_thread_ != nullptr) return false;
  running_ = true;
  writer_thread_ =
      std::make_unique<std::thread>(std::bind(Logger::writer_worker, this));
  return true;
}

bool Logger::log(
    Level level, const std::string &content,
    std::thread::id id = std::this_thread::get_id(),
    const std::source_location location = std::source_location::current()) {
  if (writer_ == nullptr || formatter_ == nullptr) return false;
  auto time = std::chrono::system_clock::now();
  auto msg = formatter_->operator()(level, content, id, location, time);
  // insert the log
  std::lock_guard lock(logs_mutex_);
  logs_.emplace_back(std::move(msg));
  // wake the writer thread
  if (logs_.size() > buf_cell_size_) logs_avail_cv_.notify_one();
  return true;
}

bool Logger::flush() {
  if (writer_ == nullptr) false;
  std::lock_guard lock(logs_mutex_);
  decltype(logs_) logs = std::move(logs_);
  // unlike the writer thread, the following writing logs should be locked to
  // avoid appending or writing logs in other threads.
  writer_logs(logs);
  writer_->flush();
  return true;
}

void Logger::destroy_writer_thread() {
  running_ = false;
  logs_avail_cv_.notify_one();
  // ensure the writer thread ends before Logger
  if (writer_thread_ != nullptr) writer_thread_->join();
}

void Logger::writer_worker() {
  while (running_) {
    std::unique_lock lock(logs_mutex_);
    logs_avail_cv_.wait(lock);
    if (running_ && !logs_.empty()) {
      decltype(logs_) logs = std::move(logs_);
      lock.unlock();
      writer_logs(logs);
      lock.lock();
    }
  }
}
