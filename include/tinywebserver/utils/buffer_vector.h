#ifndef BUFFER_VECTOR_H_
#define BUFFER_VECTOR_H_

#include <sys/uio.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <string_view>

class BufferVector {
 protected:
  struct Segment {
    char* data;
    /**
     * @brief The capacity of data.
     */

    size_t cap;
    /**
     * @brief The actual of data used as buffer.
     */

    size_t size;

    /**
     * @brief Whether data is readonly
     */
    bool readonly;

    /**
     * @brief The function to free data
     */
    std::function<void(char*, size_t)> deleter;

    Segment(size_t cap)
        : cap(cap),
          size(cap),
          readonly(false),
          deleter([](char* data, size_t size) { delete[] data; }) {
      data = new char[cap];
      if (data == nullptr) this->cap = size = 0;
    }

    Segment(char* data, size_t cap,
            std::function<void(char*, size_t)>&& deleter)
        : data(data),
          cap(size),
          size(size),
          readonly(true),
          deleter(std::move(deleter)) {}

    Segment(const Segment&) = delete;

    Segment(Segment&& obj)
        : data(obj.data),
          cap(obj.cap),
          size(obj.size),
          readonly(obj.readonly),
          deleter(std::move(obj.deleter)) {
      // clear
      obj.data = nullptr;
      obj.cap = obj.size = 0;
      obj.deleter = nullptr;
    }

    Segment& operator=(const Segment&) = delete;

    Segment& operator=(Segment&& obj) {
      data = std::move(obj.data);
      cap = obj.cap;
      size = obj.size;
      readonly = obj.readonly;
      deleter = std::move(obj.deleter);
      // clear
      obj.data = nullptr;
      obj.cap = obj.size = 0;
      obj.deleter = nullptr;
    }

    ~Segment() {
      if (deleter != nullptr) {
        deleter(data, size);
        deleter = nullptr;
      }
      data = nullptr;
      cap = size = 0;
    }

    /**
     * @brief Read data from Segment
     */
    size_t read(char* dest, size_t size, size_t begin = 0) {
      auto cnt = std::min(size, this->size - begin);
      std::copy(data + begin, data + begin + cnt, dest);
      return cnt;
    }

    /**
     * @brief Write data to Segment
     */
    size_t write(const char* src, size_t size, size_t begin = 0) {
      auto cnt = std::min(size, this->size - begin);
      std::copy(src, src + cnt, data + begin);
      return cnt;
    }
  };

 public:
  /**
   * @brief Construct the BufferVector.
   * @param capacity the initial size of the each buffer.
   */
  BufferVector(size_t capacity) : cap_(capacity) {
    data_.emplace_back(Segment(cap_));
  }

  ~BufferVector() = default;

  bool set_capacity(size_t cap) {
    if (cap == 0) return false;
    cap_ = cap;
  }

  /**
   * @brief Update the position of read pointer
   * @note If step > readable_size(), step will be set to readable_size()
   */
  void update_read_ptr(size_t step) {
    auto remain = readable_size();
    if (step >= remain) {
      clear();
      return;
    }
    while (step > 0) {
      auto& cur_data = data_[read_index_];
      auto cnt = std::min(step, cur_data.size - n_read_);
      // update step
      step -= cnt;
      // update self
      n_read_ += cnt;
      if (n_read_ == cur_data.size) forward_read();
    }
  }

  /**
   * @brief Get the writeable size
   */
  size_t writeable_size() const {
    size_t ret = data_[write_index_].size - n_write_;
    for (size_t i = write_index_ + 1; i < data_.size(); ++i)
      ret += data_[i].size;
    return ret;
  }

  /**
   * @brief Get the readable size
   */
  size_t readable_size() const {
    if (read_index_ == write_index_) return n_write_ - n_read_;
    size_t ret = data_[read_index_].size - n_read_;
    for (size_t i = read_index_ + 1; i < write_index_; ++i)
      ret += data_[i].size;
    ret += n_write_;
    return ret;
  }

  /**
   * @brief Make sure the BufferVector can write data of the specified size
   */
  void ensure_writeable(size_t size) {
    size_t remain = writeable_size();
    // Make sure there is always space left
    if (remain > size) return;
    size_t cnt = (size - remain) / cap_ + 1;
    add_segment(cnt);
  }

  /**
   * @brief Get data of the specified size from the BufferVector
   * @return Read size
   */
  ssize_t read(char* dest, size_t size) {
    auto read_size = std::min(size, readable_size());

    for (size = read_size; size > 0;) {
      auto& cur_data = data_[read_index_];
      auto cnt = cur_data.read(dest, size, n_read_);
      // update destination
      dest += cnt;
      size -= cnt;
      // update source
      n_read_ += cnt;
      if (n_read_ == cur_data.size) forward_read();
    }
    return read_size;
  }

  /**
   * @brief Write data with the specified size to the Buffer
   */
  BufferVector& write(const char* src, size_t size) {
    ensure_writeable(size);
    while (size > 0) {
      auto& cur_data = data_[write_index_];
      auto cnt = cur_data.write(src, size, n_write_);
      // update source
      src += cnt;
      size -= cnt;
      // update destination
      n_write_ += cnt;
      if (n_write_ == cur_data.size) forward_write();
    }
    return *this;
  }

  BufferVector& write(std::string_view& str) {
    return write(str.data(), str.size());
  }

  BufferVector& write(const BufferVector& buffer) {
    if (n_write_ != 0) {
      // mark Segment, it has been written
      data_[write_index_].size = n_write_;
      forward_write();
    }
    for (const auto& seg : buffer.data_) {
      Segment seg_new(seg.size);
      std::copy(seg.data, seg.data + seg.size, seg_new.data);
      data_.emplace(data_.begin() + write_index_, std::move(seg_new));
      forward_write();
    }
    return *this;
  }

  /**
   * Add existing buffer to BufferVetor. This can reduce redundant copying if we
   * use something like mmap.
   * @param deleter The function to free buffer
   */
  void add_segment(char* buffer, size_t size,
                   std::function<void(char*, size_t)>&& deleter) {
    if (n_write_ != 0) {
      // mark Segment, it has been written
      data_[write_index_].size = n_write_;
      forward_write();
    }

    data_.emplace(data_.begin() + write_index_,
                  Segment(buffer, size, std::move(deleter)));
    forward_write();
  }

  /**
   * @brief Reset the write pointer and read pointer
   */
  void clear() {
    write_index_ = read_index_ = n_read_ = n_write_ = 0;
    // remove the readonly Segemnt
    for (auto it = data_.begin(); it != data_.end(); ++it)
      if (it->readonly)
        data_.erase(it);
      else
        it->size = it->cap;
  }

  /**
   * @brief Return the iovec that can be read.
   */
  std::vector<iovec> get_read_iovec() {
    std::vector<iovec> ret;
    if (read_index_ == write_index_) {
      if (n_read_ != n_write_) {
        auto& cur = data_[read_index_];
        ret.emplace_back(iovec{.iov_base = cur.data + n_read_,
                               .iov_len = n_write_ - n_read_});
      }
      return ret;
    }
    auto& cur_read = data_[read_index_];
    ret.emplace_back(iovec{.iov_base = cur_read.data + n_read_,
                           .iov_len = cur_read.size - n_read_});
    for (size_t i = read_index_ + 1; i < write_index_; ++i)
      ret.emplace_back(
          iovec{.iov_base = data_[i].data, .iov_len = data_[i].size});
    if (n_write_ != 0) {
      auto& cur_write = data_[write_index_];
      ret.emplace_back(iovec{.iov_base = cur_write.data, .iov_len = n_write_});
    }
    return ret;
  }

  /**
   * @brief Return the iovec that can be written
   */
  std::vector<iovec> get_write_iovec() {
    if (writeable_size() == 0) return {};
    std::vector<iovec> ret;
    auto& cur_write = data_[write_index_];
    ret.emplace_back(iovec{.iov_base = cur_write.data + n_write_,
                           .iov_len = cur_write.size - n_write_});
    for (size_t i = write_index_ + 1; i < data_.size(); ++i)
      ret.emplace_back(
          iovec{.iov_base = data_[i].data, .iov_len = data_[i].size});
    return ret;
  }

 protected:
  /**
   * @brief Add segment to the data_
   */
  bool add_segment(size_t n) {
    for (size_t i = 0; i < n; ++i) data_.emplace_back(Segment(cap_));
  }

  /**
   * @brief Let the read index go one step forward
   */
  void forward_read() {
    // recycle the Segment, and the data_ looks like a ring
    auto seg = std::move(data_.front());
    data_.pop_front();
    seg.size = seg.cap;
    if (!seg.readonly) data_.emplace_back(std::move(seg));

    n_read_ = 0;
    // attention
    --write_index_;
  }

  /**
   * @brief Let the write index go one step forward
   */
  void forward_write() {
    ++write_index_;
    if (write_index_ == data_.size()) add_segment(1);
    n_write_ = 0;
  }

  /**
   * @brief the memory to store the data.
   */
  std::deque<Segment> data_;

  /**
   * @brief Capacity, the size of each buffer.
   */
  size_t cap_;

  /**
   * @brief index of reader in the data_
   */
  size_t read_index_ = 0;

  /**
   * @brief The byte that has been readed in the data_[read_index_]
   */
  size_t n_read_ = 0;

  /**
   * @brief index of reader in the data_
   */
  size_t write_index_ = 0;

  /**
   * @brief The byte that has been writen in the data_[write_index_]
   */
  size_t n_write_ = 0;
};

#endif