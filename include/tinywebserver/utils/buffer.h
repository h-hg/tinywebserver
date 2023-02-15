#ifndef BUFFER_H_
#define BUFFER_H_

#include <algorithm>
#include <memory>
#include <string_view>

/**
 * @brief A buffer using continue memory.
 */
class Buffer {
 public:
  /**
   * @brief Construct the buffer.
   * @param capacity the initial size of the Buffer
   */
  inline static const size_t default_capacity = 1024 * 4;

  Buffer(size_t capacity = default_capacity)
      : cap_(capacity),
        data_(std::make_unique<char[]>(cap_)),
        begin_ptr_(data_.get()),
        read_ptr_(begin_ptr_),
        write_ptr_(begin_ptr_) {}

  /**
   * @brief In order to reduce the occurrence of duplicate data, copy
   * constructors are not allowed.
   */
  Buffer(const Buffer&) = delete;

  Buffer(Buffer&& obj)
      : cap_(obj.cap_),
        data_(std::move(obj.data_)),
        begin_ptr_(obj.begin_ptr_),
        read_ptr_(obj.read_ptr_),
        write_ptr_(obj.write_ptr_) {
    obj.begin_ptr_ = obj.read_ptr_ = obj.write_ptr_ = nullptr;
  }

  /**
   * @brief In order to reduce the occurrence of duplicate data, copy
   * operation are not allowed.
   */
  Buffer& operator=(const Buffer&) = delete;

  Buffer& operator=(Buffer&& obj) {
    cap_ = obj.cap_;
    data_ = std::move(obj.data_);
    begin_ptr_ = obj.begin_ptr_;
    read_ptr_ = obj.read_ptr_;
    write_ptr_ = obj.write_ptr_;
    obj.begin_ptr_ = obj.read_ptr_ = obj.write_ptr_ = nullptr;
    return *this;
  }

  ~Buffer() = default;

  /**
   * @brief Get the readable size
   */
  size_t readable_size() const { return write_ptr_ - read_ptr_; }

  /**
   * @brief Determine whether the readable data is empty.
   */
  bool readable_empty() const { return write_ptr_ == read_ptr_; }

  /**
   * @brief Get the current read pointer
   */
  const char* cur_read_ptr() const { return read_ptr_; }

  /**
   * @brief Get the current read pointer
   */
  char* cur_read_ptr() { return read_ptr_; }

  /**
   * @brief Update the position of read pointer. step will be set to
   * readable_size() if step > readable_size();
   */
  char* update_read_ptr(size_t step) {
    step = std::min(step, readable_size());
    return read_ptr_ += step;
  }

  /**
   * @brief Get the view of buffer
   */
  std::string_view view() const { return {read_ptr_, write_ptr_}; }

  /**
   * @brief Get the writeable size
   */
  size_t writeable_size() const { return (begin_ptr_ + cap_) - write_ptr_; }

  /**
   * @brief Get the current write pointer
   */
  const char* cur_write_ptr() const { return write_ptr_; }

  /**
   * @brief Get the current write pointer
   */
  char* cur_write_ptr() { return write_ptr_; }

  /**
   * @brief Update the position of write pointer. step will be set to
   * writeable_size() if step > writeable_size();
   */
  char* update_write_ptr(size_t step) {
    step = std::min(step, writeable_size());
    return write_ptr_ += step;
  }

  /**
   * @brief Make sure the Buffer can write data of the specified size
   */
  void ensure_writeable(size_t size) {
    if (writeable_size() >= size) return;
    auto move_size = readable_size();
    if ((read_ptr_ - begin_ptr_) + writeable_size() > size) {
      // Move the position of the data to be readed.
      std::copy(read_ptr_, write_ptr_, begin_ptr_);
      // reset the pointer
      read_ptr_ = begin_ptr_;
      write_ptr_ = read_ptr_ + move_size;
    } else {
      // reallocate space
      auto new_size = size * 2;
      auto new_buf = std::make_unique<char[]>(new_size);
      std::copy(read_ptr_, write_ptr_, new_buf.get());
      // reset the pointer
      data_ = std::move(new_buf);
      cap_ = new_size;
      begin_ptr_ = new_buf.get();
      read_ptr_ = begin_ptr_;
      write_ptr_ = read_ptr_ + move_size;
    }
  }

  /**
   * @brief Get data of the specified size from the Buffer
   * @return Read size
   */
  ssize_t read(char* buffer, size_t size) {
    auto read_size = std::min(size, readable_size());
    // copy the data
    std::copy(read_ptr_, read_ptr_ + read_size, buffer);
    // update the pointer
    read_ptr_ += read_size;
    return read_size;
  }

  /**
   * @brief Write data with the specified size to the Buffer
   */
  Buffer& write(const char* str, size_t size) {
    ensure_writeable(size);
    // write the data
    std::copy(str, str + size, write_ptr_);
    // update write pointer
    write_ptr_ += size;
    return *this;
  }

  Buffer& write(std::string_view& str) { return write(str.data(), str.size()); }

  Buffer& write(const Buffer& buffer) {
    return write(buffer.cur_read_ptr(), buffer.readable_size());
  }

  /**
   * @brief Reset the write pointer and read pointer
   */
  void clear() { write_ptr_ = read_ptr_ = begin_ptr_; }

 protected:
  /**
   * @brief the memory to store the data.
   */
  std::unique_ptr<char[]> data_;

  /**
   * @brief Capacity, the size of data_.
   */
  size_t cap_;

  /**
   * @brief position of reader
   */
  char* read_ptr_;

  /**
   * @brief position of writer
   */
  char* write_ptr_;

  /**
   * @brief The starting position of buffer
   */
  char* begin_ptr_;
};

#endif