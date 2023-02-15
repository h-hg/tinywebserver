#ifndef RESOURCE_POOL_H_
#define RESOURCE_POOL_H_

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

template <typename Resource>
class ResourcePool {
 private:
  /**
   * @brief Constuct the resource pool.
   * @param alloc_count The number of resources allocated at one time when the
   * resource pool is empty and the total number after allocation will not
   * exceed max_count.
   * @param max_count The maximum number of resources that can be allocated.
   * @param alloc Function to allocate the resources.
   * @param deleter Function to free the resources.
   */
  ResourcePool(size_t alloc_count, size_t max_count,
               std::function<Resource *()> &&alloc,
               std::function<void(Resource *)> &&deleter)
      : alloc_count_(alloc_count),
        max_count_(max_count),
        alloc_(std::move(alloc)),
        deleter_(std::move(deleter)) {
    std::unique_lock lock(res_mutex_);
    alloc_res(res_mutex_);
  }
  ResourcePool(const ResourcePool &) = delete;
  ResourcePool(ResourcePool &&) = delete;
  ResourcePool &operator=(const ResourcePool &) = delete;
  ResourcePool &operator=(ResourcePool &&) = delete;

  /**
   * @brief The implementation of get_instance()
   */
  ResourcePool<Resource> &get_instance_impl(
      size_t alloc_count, size_t max_count, std::function<Resource *()> &&alloc,
      std::function<void(Resource *)> &&deleter) {
    static ResourcePool<Resource> instance(
        alloc_count, max_count, std::move(alloc), std::move(deleter));
    return instance;
  }

 public:
  /**
   * @brief Init the instance
   * @param alloc_count The number of resources allocated at one time when the
   * resource pool is empty and the total number after allocation will not
   * exceed max_count.
   * @param max_count The maximum number of resources that can be allocated.
   * @param alloc Function to allocate the resources.
   * @param deleter Function to free the resources.
   */
  static bool init(size_t alloc_count, size_t max_count,
                   std::function<Resource *()> &&alloc,
                   std::function<void(Resource *)> &&deleter) {
    if (alloc_count == 0 || max_count == 0 || alloc == nullptr ||
        deleter == nullptr)
      return false;
    get_instance_impl(alloc_count, max_count, std::move(alloc),
                      std::move(deleter));
    return true;
  }

  /**
   * @brief A factory method to get the instance of ResourcePool. The instance
   * is designed in singleton pattern. You must call the init function before
   * calling this method for the first time. Otherwise some default value will
   * be used. The default value of min_count is 8, max_count is 64, allocation
   * is the new operation, deleter is delete operation.
   */
  static ResourcePool<Resource> &get_instance() {
    return get_instance_impl(
        8, 64, []() { return new Resource; },
        [](Resource *ptr) { delete ptr; });
  }

  /**
   * @brief Set the alloc_count.
   */
  bool set_alloc_count(size_t alloc_count) {
    if (alloc_count == 0) return false;
    std::lock_guard lock(res_mutex_);
    alloc_count_ = alloc_count;
    return true;
  }

  /**
   * @brief Set the max_count.
   */
  bool set_max_count(size_t max_count) {
    std::lock_guard lock(res_mutex_);
    max_count_ = max_count;
    return true;
  }

  /**
   * @brief Destroy the all the resources.
   */
  ~ResourcePool() {
    std::lock_guard lock(res_mutex_);
    while (!res_.empty()) {
      auto p = res_.front();
      res_.pop();
      deleter_(p);
    }
  }

  /**
   * @brief Get the number of resources that are in the pool. The return value
   * may be greater than max_count if max_count is reset to a smaller value.
   * However, resources exceeding max_count will be destroyed in subsequent
   * operations.
   */
  size_t get_free_resource_count() const {
    std::lock_guard lock(res_mutex_);
    return res_.size();
  }

  /**
   * @brief Get the number of total resources that haved allocated. The return
   * value may be greater than max_count if max_count is reset to a smaller
   * value. However, resources exceeding max_count will be destroyed in
   * subsequent operations.
   */
  size_t get_total_resource_count() const {
    std::lock_guard lock(res_mutex_);
    return total_count_;
  }

  /**
   * @brief Get the maximum number of resources that can be allocated
   */
  size_t get_max_resource_count() const { return max_count_; }

  /**
   * @brief Get the minimum of number of resources in the pool.
   */
  size_t get_min_resource_count() const { return alloc_count_; }

  /**
   * @brief Get the resource
   * @todo The lifetime of the ResourcePool should be longer than the return
   * value.
   */
  std::shared_ptr<Resource> get() {
    std::unique_lock lock(res_mutex_);
    free_res(lock);
    if (res_.empty()) alloc_res(lock);
    if (res_.empty()) return nullptr;
    auto p = res_.front();
    res_.pop();
    return std::shared_ptr<Resource>(p, std::bind(ResourcePool::recycle, this));
  }

 protected:
  /**
   * @brief Allocate resources.
   */
  void alloc_res(std::unique_lock<std::mutex> &lock) {
    // check the lock
    assert(lock.mutex() == &res_mutex_);
    assert(lock.owns_lock());

    if (total_count_ >= max_count_) return;
    for (size_t i = 0,
                n = std::min(alloc_count_, max_count_ - size_t(total_count_));
         i < n; ++i) {
      res_.push(alloc_());
      ++total_count_;
    }
  }

  /**
   * @brief Free the resources that exceeding max_count_
   */
  void free_res(std::unique_lock<std::mutex> &lock) {
    // check the lock
    assert(lock.mutex() == &res_mutex_);
    assert(lock.owns_lock());

    if (total_count_ < max_count_) return;
    for (size_t i = 0, n = std::min(res_.size(), total_count_ - max_count_);
         i < n; ++i) {
      auto p = res_.front();
      res_.pop();
      deleter_(p);
      --total_count_;
    }
  }

  /**
   * @brief Recyle the resource into pool.
   */
  void recycle(Resource *ptr) {
    std::lock_guard lock(res_mutex_);
    res_.push(ptr);
  }

  /**
   * @brief Function to allocate the resources.
   */
  std::function<Resource *()> alloc_;

  /**
   * @brief Function to free the resources.
   */
  std::function<void(Resource *)> deleter_;

  /**
   * @brief Mutex for resources
   */
  mutable std::mutex res_mutex_ = {};

  /**
   * @brief resources pool
   */
  std::queue<Resource *> res_ = {};

  /**
   * @brief The number of resources that have been used.
   */
  std::atomic<size_t> total_count_{0};

  /**
   * @brief The maximum number of resources that can be allocated.
   */
  size_t max_count_ = 0;

  /**
   * @brief The number of resources allocated at one time when the resource pool
   * is empty and the total number after allocation will not exceed max_count.
   */
  size_t alloc_count_ = 0;
};

#endif