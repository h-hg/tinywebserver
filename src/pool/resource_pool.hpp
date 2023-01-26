#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

template <typename Resource>
class ResourcePool {
 private:

  /**
   * @brief Constuct the resource pool.
   * @param min_count The minimum of number of resources in the pool.
   * @param max_count The maximum number of resources that can be allocated.
   * @param alloc Function to allocate the resources.
   * @param deleter Function to free the resources.
   */
  ResourcePool(size_t min_count, size_t max_count,
               const std::function<Resource *()> &alloc,
               const std::function<void(Resource *)> &deleter)
      : min_count_(min_count),
        max_count_(max_count),
        alloc_(alloc),
        deleter_(deleter) {
    alloc_res();
  }
  ResourcePool(const ResourcePool &) = delete;
  ResourcePool(ResourcePool &&) = delete;
  ResourcePool &operator=(const ResourcePool &) = delete;
  ResourcePool &operator=(ResourcePool &&) = delete;

 public:

  /**
   * @brief A factory method to get the instance of ResourcePool. The instance
   * is designed in singleton pattern, so the arguments are only valid on the
   * first call.
   * @param min_count The minimum of number of resources in the pool.
   * @param max_count The maximum number of resources that can be allocated.
   * @param alloc Function to allocate the resources.
   * @param deleter Function to free the resources.
   */
  static std::shared_ptr<ResourcePool<Resource>> get_instance(
      size_t min_count, size_t max_count,
      const std::function<Resource *()> &alloc,
      const std::function<void(Resource *)> &deleter);

  /**
   * @brief Destroy the all the resources.
   */
  ~ResourcePool();

  /**
   * @brief Get the number of resources that are in the pool
   */
  size_t get_free_resource_count() const {
    std::lock_guard lock(res_mutex_);
    return res_.size();
  }

  /**
   * @brief Get the number of total resources that haved allocated
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
  size_t get_min_resource_count() const { return min_count_; }

  /**
   * @brief Get the resource
   * @todo The lifetime of the ResourcePool should be longer than the return
   * value.
   */
  std::shared_ptr<Resource> get();

 protected:
  /**
   * @brief Allocate resources.
   */
  void alloc_res();

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
  const std::function<Resource *()> &alloc_;

  /**
   * @brief Function to free the resources.
   */
  const std::function<void(Resource *)> &deleter_;

  /**
   * @brief Mutex for resources
   */
  std::mutex res_mutex_ = {};

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
   * @brief The minimum of number of resources in the pool. It should be noted
   * that the min_count_ + res_.size() <= res_total_count_ .
   */
  size_t min_count_ = 0;
};

template <typename Resource>
void ResourcePool<Resource>::alloc_res() {
  std::lock_guard lock(res_mutex_);
  for (size_t i = 0,
              n = std::min(min_count_, max_count_ - size_t(total_count_));
       i < n; ++i) {
    ++total_count_;
    res_.push(alloc_());
  }
}

template <typename Resource>
std::shared_ptr<Resource> ResourcePool<Resource>::get() {
  std::lock_guard lock(res_mutex_);
  if (total_count_ >= max_count_) return nullptr;
  if (res_.empty()) alloc_res();
  auto p = res_.front();
  return std::shared_ptr<Resource>(p, std::bind(ResourcePool::recycle, this));
}

template <typename Resource>
std::shared_ptr<ResourcePool<Resource>> ResourcePool<Resource>::get_instance(
    size_t min_count, size_t max_count,
    const std::function<Resource *()> &alloc,
    const std::function<void(Resource *)> &deleter) {
  static std::once_flag flag;
  static std::shared_ptr<ResourcePool<Resource>> ptr = nullptr;
  std::call_once(flag, [&]() {
    ptr = std::make_shared<ResourcePool<Resource>>(min_count, max_count, alloc,
                                                   deleter);
  });
  return ptr;
}

template <typename Resource>
ResourcePool<Resource>::~ResourcePool<Resource>() {
  std::lock_guard lock(res_mutex_);
  while (!res_.empty()) {
    auto p = res_.front();
    res_.pop();
    deleter_(p);
  }
}