#include "tinywebserver/network/http/handler.h"

#include <algorithm>
namespace http {

bool HandlerManager::handle(const std::string &pattern, HTTPHandler &&handler) {
  if (pattern.empty() || handler == nullptr || pattern2handler_.count(pattern))
    return false;
  auto sptr = std::make_unique<HTTPHandler>(std::move(handler));
  auto ptr = sptr.get();
  pattern2handler_.emplace(pattern, std::move(sptr));
  if (pattern.ends_with('/')) {
    // find the first element whose size < pattern.size()
    auto it = std::lower_bound(
        handlers_.begin(), handlers_.end(), pattern.size(),
        [](const auto &p, auto value) { return p.first.size() > value; });
    handlers_.emplace(it, pattern, ptr);
  }
  return true;
}

HTTPHandler *HandlerManager::match(const std::string &pattern) const {
  if (auto it = pattern2handler_.find(pattern); it != pattern2handler_.end())
    return it->second.get();
  for (const auto &[p, handler] : handlers_)
    if (pattern.starts_with(p)) return handler;
  return nullptr;
}
}  // namespace http
