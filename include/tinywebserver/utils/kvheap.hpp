#ifndef KVHEAP_H_
#define KVHEAP_H_

#include <algorithm>
#include <cassert>
#include <functional>
#include <initializer_list>
#include <new>
#include <unordered_map>
#include <vector>

/**
 * @brief A key-value based heap structure. Different to std::priority_queue,
 * you can access or update the element in the heap by key.
 */
template <typename Key, typename Value, typename Compare = std::less<Value>>
class KVHeap {
 protected:
  /**
   * @brief The node structure in the heap.
   */
  class Node {
   public:
    template <typename... Args>
    Node(Key const &key, Args &&...args)
        : value(std::forward<Args>(args)...), key(key) {}
    Key key;
    Value value;
  };

 public:
  KVHeap(const Compare &compare = Compare()) : cmp_(compare) {}

  template <typename InputIt>
  KVHeap(InputIt first, InputIt last, const Compare &compare = Compare())
      : KVHeap(compare) {
    for (; first != last; ++first) push(first->first, first->second);
  }

  KVHeap(std::initializer_list<std::pair<Key, Value>> list,
         const Compare &compare = Compare())
      : KVHeap(list.begin(), list.end(), compare) {}

  ~KVHeap() = default;

  /**
   * @note Return const reference to avoid being modified by callers.
   */
  const Value &top() const { return heap_[0].value; }

  /**
   * @return The key and the value.
   */
  std::pair<Key, Value> pop() {
    std::pair<Key, Value> ret = {heap_.front().key,
                                 std::move(heap_.front().value)};
    earse_by_index(0);
    return ret;
  }

  size_t size() const { return heap_.size(); }

  bool empty() const { return heap_.size() == 0; }

  size_t count(Key const &key) const { return key2index_.count(key); }

  void clear() {
    heap_.clear();
    key2index_.clear();
  }

  /**
   * @brief Access the value by key
   */
  const Value &get(Key const &key) const {
    if (auto it = key2index_.find(key); it == key2index_.end())
      return {};
    else
      return heap_[it->second].value;
  }

  /**
   * @brief Access the value by index.
   */
  const Value &operator[](size_t index) const { return heap_[index].value; }

  bool push(Key const &key, const Value &elem) {
    if (key2index_.count(key)) return false;
    auto index = heap_.size();
    key2index_[key] = index;
    heap_.emplace_back(key, elem);
    sift_up(index, heap_.size());
    return true;
  }

  bool push(Key const &key, Value &&elem) {
    if (key2index_.count(key)) return false;
    auto index = heap_.size();
    key2index_[key] = index;
    heap_.emplace_back(key, std::move(elem));
    sift_up(index, heap_.size());
    return true;
  }

  template <typename... Args>
  bool emplace(Key const &key, Args &&...args) {
    if (key2index_.count(key)) return false;
    auto index = heap_.size();
    key2index_[key] = index;
    heap_.emplace_back(key, std::forward<Args>(args)...);
    sift_up(index, heap_.size());
    return true;
  }

  bool erase(Key const &key) {
    auto it = key2index_.find(key);
    if (it == key2index_.end()) return false;
    earse_by_index(it->second);
    return true;
  }

  bool update(Key const &key, Value &&obj) {
    auto it = key2index_.find(key);
    if (it == key2index_.end()) return false;
    auto index = it->second;
    heap_[index].value = std::move(obj);
    update_by_index(index, heap_.size());
    return true;
  }

  bool update(Key const &key, const Value &&obj) {
    auto it = key2index_.find(key);
    if (it == key2index_.end()) return false;
    auto index = it->second;
    heap_[index].value = obj;
    update_by_index(index, heap_.size());
    return true;
  }

  bool update(Key const &key,
              const std::function<void(Value &obj)> &call_back) {
    auto it = key2index_.find(key);
    if (it == key2index_.end()) return false;
    auto index = it->second;
    call_back(heap_[index].value);
    update_by_index(index, heap_.size());
    return true;
  }

  /**
   * @brief Update all the element.
   */
  void update(const std::function<void(Value &obj)> &call_back) {
    for (auto it = heap_.begin(); it != heap_.end(); ++it) call_back(it->value);
    for (size_t i = 1; i < heap_.size(); ++i) {
      sift_up(i, i + 1);
    }
  }

 protected:
  constexpr size_t left_chil(size_t i) { return 2 * i + 1; }

  constexpr size_t right_child(size_t i) { return 2 * i + 2; }

  constexpr size_t parent(size_t i) { return (i - 1) / 2; }

  constexpr size_t level_next(size_t i) { return i + 1; }

  void update_by_index(size_t index, size_t n) {
    assert(index < n);
    assert(n <= heap_.size());
    if (n == 1) return;
    if (!sift_down(index, n)) sift_up(index, n);
  }

  void earse_by_index(size_t index) {
    assert(index < heap_.size());
    size_t n = heap_.size() - 1;
    if (index == n) {
      key2index_.erase(heap_.back().key);
      heap_.pop_back();
    } else {
      swap_node(index, n);
      key2index_.erase(heap_.back().key);
      heap_.pop_back();
      sift_down(index, heap_.size());
    }
  }

  /**
   * @brief
   * @param n The size of heap, 只使用 heap_ 中 [0, n) 这个区间。
   */
  bool sift_down(size_t index, size_t n) {
    assert(index >= 0 && index < n);
    assert(n <= heap_.size());

    size_t i = index;
    for (auto lc = left_chil(i); lc < n; lc = left_chil(i)) {
      auto child = lc;
      if (auto rc = level_next(lc);
          rc < n && cmp_(heap_[lc].value, heap_[rc].value))
        child = rc;
      if (!cmp_(heap_[i].value, heap_[child].value)) break;
      swap_node(i, child);
      i = child;
    }
    return i > index;
  }

  bool sift_up(size_t index) {
    assert(index >= 0 && index < heap_.size());
    auto i = index;
    for (auto p = parent(i); i > 0 && cmp_(heap_[p].value, heap_[i].value);
         p = parent(i)) {
      swap_node(i, p);
      i = parent(i);
    }
    return i < index;
  }

  /**
   * @param i index of heap_
   * @param j index of heap_
   */
  void swap_node(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    if (i == j) return;
    std::swap(heap_[i], heap_[j]);
    // update the key to index
    key2index_[heap_[i].key] = i;
    key2index_[heap_[j].key] = j;
  }

  /**
   * @brief Compare
   */
  Compare cmp_;

  /**
   * @brief Key to the index of heap_
   */
  std::unordered_map<Key, size_t> key2index_;

  /**
   * @brief Container
   */
  std::vector<Node> heap_;
};

#endif
