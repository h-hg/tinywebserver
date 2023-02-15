#ifndef HTTP_RESPONSE_WRITER_
#define HTTP_RESPONSE_WRITER_

#include <sys/mman.h>
#include <sys/stat.h>

#include "tinywebserver/network/http/response.h"
#include "tinywebserver/utils/buffer_vector.h"

namespace http {

class Connection;

class ResponseWriter {
  friend Connection;

 public:
  ResponseWriter() = default;

  ~ResponseWriter() = default;

  ResponseWriter(const ResponseWriter&) = delete;

  ResponseWriter(ResponseWriter&& obj)
      : resp_(std::move(obj.resp_)), buf_(std::move(obj.buf_)) {
    obj.clear();
  }

  ResponseWriter& operator=(const ResponseWriter&) = delete;

  ResponseWriter& operator=(ResponseWriter&& obj) {
    if (this == &obj) return *this;
    resp_ = std::move(obj.resp_);
    buf_ = std::move(obj.buf_);

    obj.clear();
    return *this;
  }

  std::string version() { return resp_.version(); }
  void set_version(const std::string& version) { resp_.set_version(version); }

  int status() { return resp_.status(); }
  void set_status(int status) { resp_.set_status(status); }

  std::string desc() { return resp_.desc(); }
  void set_desc(const std::string& desc) { resp_.set_desc(desc); }

  Header& header() { return resp_.header(); }
  const Header& header() const { return resp_.header(); }

  /**
   * @brief Write data to the body of reponse.
   */
  void write(char* src, size_t size) { buf_.write(src, size); }

  void write(std::string_view content) { buf_.write(content); }

  /**
   * @brief Add existing buffer to body of reponse. This can reduce redundant
   * copying if we use something like mmap.
   * @param deleter The function to free buffer
   */
  void write(char* buffer, size_t size,
             std::function<void(char*, size_t)>&& deleter,
             bool readonly = true) {
    buf_.write(buffer, size, std::move(deleter), readonly);
  }

  void clear() {
    resp_.clear();
    buf_.clear();
  }

 protected:
  Response resp_;
  BufferVector buf_;
};

}  // namespace http

#endif