#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <cstddef>
#include <cstdlib>
#include <new>

/**
 * @brief A simple memory pool
 * @ref The std::alloc in SGI STL, G2.92.
 */
class MemoryPool {
 protected:
  struct Object {
    Object *next;
  };

  /**
   * @brief The number of free list, each free list is used to allocate
   * different sizes of memory
   */
  static const int n_free_lists_ = 16;

  /**
   * @brief
   */
  static const int base_ = 8;

  static const int max_ = n_free_lists_ * base_;

  /**
   * @brief free_lists_[i] is used to allocate memory with a size in ( i *
   * base_, (i+1) * base_ ]
   */
  inline static Object *volatile free_lists_[n_free_lists_] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  };

  /**
   * @brief The starting address of the free memory block.
   */
  inline static char *start_free_ = nullptr;

  /**
   * @brief The ending address of the free memory block.
   */
  inline static char *end_free_ = nullptr;

  /**
   * @brief The number of bytes that have been allocated.
   */
  inline static size_t heap_size_ = 0;

  /**
   * @brief Get the index of free_lists_ with objecs of specific bytes.
   * @param The number of bytes of the object.
   */
  static std::size_t get_free_list_index(std::size_t nbytes) {
    // return nbytes % base_ == 0 ? nbytes / base_ + 1 : nbytes / base_;
    return (nbytes + base_ - 1) / base_ - 1;
  }

  /**
   * @brief Round up nbytes to the nearest multiple of base_.
   */
  static size_t round_up(size_t nbytes) {
    return (nbytes + base_ - 1) & ~(base_ - 1);
  }

  /**
   * @brief Allocates a chunk for n_jobs objects of size nbytes.
   * n_objs may be reduced if it is unable to allocate the requested
   * number.
   */
  static char *chunk_alloc(std::size_t nbytes, int &n_jobs);

  /**
   * @brief Refill the free list that allocates the object with size nbytes.
   * This function will be called when corresponding free list is empty.
   * @param nbytes We assume that nbytes is properly aligned. We hold the
   * allocation lock
   * @return Return an object with nbytes bytes.
   */
  static void *refill(size_t nbytes) {
    // A preset value indicating how many objects will be applied for at one
    // time. It may not be possible to apply for so many objects actually.
    int n_objs = 20;

    void *chunk = chunk_alloc(nbytes, n_objs);  // n_objs is pass-by-referene
    if (n_objs == 1) return chunk;
    Object *volatile *p_free_list = free_lists_ + get_free_list_index(nbytes);
    auto ret = reinterpret_cast<Object *>(chunk);

    // build free list on the chunk and set p_free_list
    auto p_next_obj = *p_free_list = reinterpret_cast<Object *>(chunk + nbytes);
    for (int i = 1; true; ++i) {  // i start from 1 to skip the first object
      auto p_current_obj = p_next_obj;
      p_next_obj = reinterpret_cast<Object *>(
          reinterpret_cast<char *>(p_next_obj) + nbytes);
      if (i + 1 == n_objs) {
        // The last one
        p_current_obj->next = nullptr;
        break;
      } else {
        p_current_obj->next = p_next_obj;
      }
    }

    return ret;
  }

  static char *chunk_alloc(size_t nbytes, int &n_objs) {
    char *ret;
    size_t total_bytes = nbytes * n_objs;
    size_t bytes_left = end_free_ - start_free_;

    if (bytes_left >= total_bytes) {
      // All the objects can be allocated by the memory piece
      ret = start_free_;
      start_free_ += total_bytes;  // update the start_free_
      return ret;
    } else if (bytes_left >= nbytes) {
      // At least one object can be allocated by the memory piece.
      n_objs = bytes_left / nbytes;  // change the n_objs
      total_bytes = nbytes * n_objs;
      ret = start_free_;
      start_free_ += total_bytes;  // update the start_free_
      return ret;
    } else {
      size_t bytes_to_get = 2 * total_bytes + round_up(heap_size_ >> 4);
      // Try to make use of the left-over piece.
      if (bytes_left > 0) {
        // Add the left-over piece to the corresponding free list.
        Object *volatile *p_free_list =
            free_lists_ + get_free_list_index(bytes_left);

        reinterpret_cast<Object *>(start_free_)->next = *p_free_list;
        *p_free_list = reinterpret_cast<Object *>(start_free_);
      }
      // Try to requst memory from system.
      start_free_ = reinterpret_cast<char *>(malloc(bytes_to_get));
      if (start_free_ == nullptr) {
        // Try to make do with what we have. That can't hurt.
        // We do not try smaller requests, since that tends to result in
        // disaster on multi-process machines. We try to get memory piece from
        // other free lists that can allocate the object with more than nbytes
        // bytes.
        for (int i = nbytes; i <= max_; i += base_) {
          Object *volatile *p_other_free_list =
              free_lists_ + get_free_list_index(i);
          auto other_free_list = *p_other_free_list;
          if (other_free_list != nullptr) {
            // put the memory piece from other free list into free memory block
            *p_other_free_list = other_free_list->next;
            start_free_ = reinterpret_cast<char *>(other_free_list);
            end_free_ = start_free_ + i;
            // try to chunk_alloc again
            return chunk_alloc(nbytes, n_objs);
          }
        }
        // It means that the 2nd level allocator has been unable to allocate any
        // memory.
        end_free_ = nullptr;
        throw std::bad_alloc();
      }
      heap_size_ += bytes_to_get;
      end_free_ = start_free_ + bytes_to_get;
      return chunk_alloc(nbytes, n_objs);
    }
  }

 public:
  /**
   * @brief Allocate the memory.
   * @param nbytes Number of bytes
   */
  static void *allocate(std::size_t nbytes) {
    // The first level Allocator
    if (nbytes > max_) return malloc(nbytes);
    // The second level Allocator
    Object *volatile *p_free_list = free_lists_ + get_free_list_index(nbytes);
    auto ret = *p_free_list;
    if (ret == nullptr) {
      void *r = refill(round_up(nbytes));
      return r;
    }
    *p_free_list = ret->next;
    return ret;
  }
  static void deallocate(void *ptr, std::size_t count) {
    // The frist level Allocator
    if (count > n_free_lists_ * base_) free(ptr);
    // The second level Allocator
    auto p_object = reinterpret_cast<Object *>(ptr);
    Object *volatile *p_free_list = free_lists_ + get_free_list_index(count);
    p_object->next = *p_free_list;
    *p_free_list = p_object;
  }
};

template <typename T, typename MemoryPool>
class SimpleAllocator {
 public:
  static T *allocate(size_t n) {
    return 0 == n ? 0 : (T *)MemoryPool::allocate(n * sizeof(T));
  }
  static T *allocate(void) { return (T *)MemoryPool::allocate(sizeof(T)); }
  static void deallocate(T *p, size_t n) {
    if (0 != n) MemoryPool::deallocate(p, n * sizeof(T));
  }
  static void deallocate(T *p) { MemoryPool::deallocate(p, sizeof(T)); }
};

#endif