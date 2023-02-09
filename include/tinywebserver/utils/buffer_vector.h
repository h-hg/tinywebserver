#ifndef BUFFERVECTOR_H_
#define BUFFERVECTOR_H_

#include <sys/uio.h>

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <string_view>

class BufferVector {
 protected:
  struct Segment {
    static inline const std::function<char*(size_t size)> default_alloc =
        [](size_t size) -> char* { return new char[size]; };

    static inline const std::function<void(char*, size_t)> default_free =
        [](char* ptr, size_t size) { delete[] ptr; };

    /**
     * @brief Pointer to the data.
     */
    char* data;

    /**
     * @brief The total capacity in byte of data.
     */
    size_t cap;

    /**
     * @brief The position whether data begins to be used.
     */
    char* begin;

    /**
     * @brief The number of bytes actually available for using.
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
        : cap(cap), size(cap), readonly(false), deleter(default_free) {
      begin = data = new char[cap];
      if (data == nullptr) {
        this->cap = size = 0;
        readonly = true;
      };
    }

    Segment(char* data, size_t cap,
            std::function<void(char*, size_t)>&& deleter, bool readonly = true)
        : data(data),
          cap(cap),
          begin(data),
          size(cap),
          readonly(readonly),
          deleter(std::move(deleter)) {}

    Segment(const Segment&) = delete;

    Segment(Segment&& obj)
        : data(obj.data),
          cap(obj.cap),
          begin(obj.begin),
          size(obj.size),
          readonly(obj.readonly),
          deleter(std::move(obj.deleter)) {
      obj.destroy();
    }

    Segment& operator=(const Segment&) = delete;

    Segment& operator=(Segment&& obj) {
      data = obj.data;
      cap = obj.cap;
      begin = obj.begin;
      size = obj.size;
      readonly = obj.readonly;
      deleter = std::move(obj.deleter);
      // clear
      obj.destroy();
      return *this;
    }

    ~Segment() { destroy(); }

    /**
     * @brief Destroy the Segment memory.
     */
    void destroy() {
      if (deleter != nullptr) {
        deleter(data, size);
        deleter = nullptr;
      }
      begin = data = nullptr;
      cap = size = 0;
      readonly = true;
    }

    /**
     * @brief Clear the data for rewriting.
     * @return Return false if the Segment is readonly or it has not buffer.
     */
    bool clear() {
      if (readonly || data == nullptr) return false;
      begin = data;
      size = cap;
      return true;
    }

    /**
     * @brief Read data from Segment
     */
    size_t read(char* dest, size_t size, size_t offset = 0) {
      if (data == nullptr) return 0;
      auto cnt = std::min(size, this->size - offset);
      std::copy(begin + offset, begin + offset + cnt, dest);
      return cnt;
    }

    /**
     * @brief Write data to Segment
     */
    size_t write(const char* src, size_t size, size_t offset = 0) {
      if (data == nullptr) return 0;
      auto cnt = std::min(size, this->size - offset);
      std::copy(src, src + cnt, begin + offset);
      return cnt;
    }

    /**
     * @brief To change the position of begin position. It does not perform any
     * copy operations, just move the pointer.
     * @return Return false if the position is invalid.
     */
    bool offset(ssize_t offset) {
      if (begin + offset < data || begin + offset > data + cap) return false;
      begin += offset;
      return true;
    }

    operator iovec() const { return iovec{.iov_base = begin, .iov_len = size}; }
  };

  using Container = std::list<Segment>;
  using Iterator = Container::iterator;
  using CIterator = Container::const_iterator;

 public:
  inline static size_t default_capacity = 1024 * 4;

  /**
   * @brief Construct the BufferVector.
   * @param capacity the initial size of the each buffer.
   */
  BufferVector(size_t capacity = default_capacity) : cap_(capacity) {
    it_write_ = data_.begin();
    add_segment(1);
  }

  BufferVector(BufferVector&& obj)
      : data_(std::move(obj.data_)),
        cap_(obj.cap_),
        n_read_(obj.n_read_),
        n_write_(obj.n_write_),
        it_write_(obj.it_write_) {
    obj.destory();
  }

  ~BufferVector() { destory(); }

  bool set_capacity(size_t cap) {
    if (cap == 0) return false;
    cap_ = cap;
    return true;
  }

  /**
   * @brief Get the writeable size.
   */
  size_t writeable_size() const {
    if (data_.empty()) return 0;
    CIterator it_write_ = this->it_write_;
    size_t ret = it_write_->size - n_write_;
    for (auto it = std::next(it_write_); it != data_.cend(); ++it)
      ret += it->size;
    return ret;
  }

  /**
   * @brief Determine whether the readable data is empty.
   */
  bool readable_empty() const {
    return data_.empty() || (it_write_ == data_.begin() && n_write_ == n_read_);
  }

  /**
   * @brief Get the readable size
   */
  size_t readable_size() const {
    if (data_.empty()) return 0;
    CIterator it_write_ = this->it_write_;
    size_t ret = 0;
    for (auto it = data_.cbegin(); it != it_write_; ++it) ret += it->size;
    return ret + n_write_ - n_read_;
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
      auto it = data_.begin();
      auto cnt = std::min(step, it->size - n_read_);
      // update step
      step -= cnt;
      // update self
      n_read_ += cnt;
      if (n_read_ == it->size) forward_reader();
    }
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
      auto cnt = data_.front().read(dest, size, n_read_);
      // update destination
      dest += cnt;
      size -= cnt;
      // update source
      n_read_ += cnt;
      if (n_read_ == data_.front().size) forward_reader();
    }
    return read_size;
  }

  /**
   * @brief Write data with the specified size to the Buffer
   */
  BufferVector& write(const char* src, size_t size) {
    ensure_writeable(size);
    while (size > 0) {
      auto cnt = it_write_->write(src, size, n_write_);
      // update source
      src += cnt;
      size -= cnt;
      // update destination
      n_write_ += cnt;
      if (n_write_ == it_write_->size) forward_writer();
    }
    return *this;
  }

  BufferVector& write(std::string_view str) {
    return write(str.data(), str.size());
  }

  /**
   * @brief Write the readable data of other BufferVector
   */
  BufferVector& write(BufferVector& buffer) {
    if (buffer.readable_empty()) return *this;
    mark_current_full_written();

    buffer.data_.front().offset(buffer.n_read_);

    auto it_end = buffer.it_write_;
    if (buffer.n_write_ != 0) {
      buffer.it_write_->size = buffer.it_write_ == buffer.data_.begin()
                                   ? buffer.n_write_ - buffer.n_read_
                                   : buffer.n_write_;
      ++it_end;
    }

    while (buffer.data_.begin() != it_end) {
      it_write_ = data_.emplace(it_write_, std::move(buffer.data_.front()));
      buffer.data_.pop_front();
    }
    forward_writer();

    // set the buffer
    buffer.n_read_ = buffer.n_write_ = 0;
    buffer.it_write_ = it_end;
    if (buffer.it_write_ == buffer.data_.end()) buffer.add_segment(1);

    return *this;
  }

  /**
   * @brief Add existing buffer to BufferVetor. This can reduce redundant
   * copying if we use something like mmap.
   * @param deleter The function to free buffer
   */
  void write(char* buffer, size_t size,
             std::function<void(char*, size_t)>&& deleter,
             bool readonly = true) {
    mark_current_full_written();
    it_write_ = data_.emplace(
        it_write_, Segment(buffer, size, std::move(deleter), readonly));
    forward_writer();
  }

  /**
   * @brief Reset the write pointer and read pointer
   */
  void clear() {
    n_read_ = n_write_ = 0;
    for (auto it = data_.begin(); it != data_.end();) {
      if (it->clear() == false) {
        auto iter = it++;
        data_.erase(iter);
      } else
        ++it;
    }
    it_write_ = data_.begin();
  }

  /**
   * @brief Return the iovec that can be read.
   */
  std::vector<iovec> get_read_iovec() const {
    if (readable_empty()) return {};
    CIterator it_write_ = this->it_write_;
    if (it_write_ == data_.cbegin())
      return {iovec{.iov_base = data_.front().begin + n_read_,
                    .iov_len = n_write_ - n_read_}};
    std::vector<iovec> ret;
    ret.emplace_back(iovec{.iov_base = data_.front().begin + n_read_,
                           .iov_len = data_.front().size - n_read_});
    for (auto it = std::next(data_.cbegin()); it != it_write_; ++it)
      ret.emplace_back(static_cast<iovec>(*it));

    if (n_write_ != 0)
      ret.emplace_back(
          iovec{.iov_base = it_write_->begin, .iov_len = n_write_});

    return ret;
  }

  /**
   * @brief Return the iovec that can be written
   */
  std::vector<iovec> get_write_iovec() const {
    if (writeable_size() == 0) return {};
    std::vector<iovec> ret;
    CIterator it_write_ = this->it_write_;
    ret.emplace_back(iovec{.iov_base = it_write_->begin + n_write_,
                           .iov_len = it_write_->size - n_write_});
    for (auto it = std::next(it_write_); it != data_.cend(); ++it)
      ret.emplace_back(static_cast<iovec>(*it));
    return ret;
  }

 protected:
  /**
   * @brief Destory BufferVector memory
   */
  void destory() {
    data_.clear();
    n_write_ = n_read_ = 0;
    it_write_ = data_.end();
  }

  /**
   * @brief mark Segment index by it_write_ has been fully written
   */
  void mark_current_full_written() {
    if (data_.empty() || n_write_ == 0) return;
    it_write_->size = n_write_;
    forward_writer();
  }

  /**
   * @brief Add Segment to the data_. This function will update it_write_
   * appropriately.
   * @param n The number of Segment.
   */
  bool add_segment(size_t n) {
    bool is_end = it_write_ == data_.end();
    if (is_end) it_write_ = data_.emplace(it_write_, Segment(cap_));
    for (size_t i = is_end ? 1 : 0; i < n; ++i)
      data_.emplace_back(Segment(cap_));
    return true;
  }

  /**
   * @brief Let the read index go one step forward
   */
  void forward_reader() {
    // recycle the Segment, and the data_ looks like a ring
    auto seg = std::move(data_.front());
    data_.pop_front();
    if (seg.clear()) data_.emplace_back(std::move(seg));
    n_read_ = 0;
  }

  /**
   * @brief Let the write index go one step forward
   */
  void forward_writer() {
    if (n_write_ == 0) return;
    ++it_write_;
    if (it_write_ == data_.end()) add_segment(1);
    n_write_ = 0;
  }

  /**
   * @brief the memory to store the data.
   */
  Container data_;

  /**
   * @brief Capacity, the size of each Segemnt.
   */
  size_t cap_;

  /**
   * @brief The byte that has been readed in the data_.front()
   */
  size_t n_read_ = 0;

  /**
   * @brief The iterator of writer in the data_. The iterator of reader is
   * always data_.begin().
   */
  Iterator it_write_;

  /**
   * @brief The byte that has been writen in the *it_write
   */
  size_t n_write_ = 0;
};

#endif
