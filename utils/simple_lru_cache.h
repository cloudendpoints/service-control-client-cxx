// For inclusion in .h files.  The real class definition is in
// simple_lru_cache_inl.h.

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_SIMPLE_LRU_CACHE_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_SIMPLE_LRU_CACHE_H_

#include <functional>
#include <unordered_map>  // for hash<>

namespace google {
namespace service_control_client {

namespace internal {
template <typename T>
struct SimpleLRUHash : public std::hash<T> {};
}  // namespace internal

template <typename Key, typename Value,
          typename H = internal::SimpleLRUHash<Key>,
          typename EQ = std::equal_to<Key> >
class SimpleLRUCache;

// Deleter is a functor that defines how to delete a Value*. That is, it
// contains a public method:
//  operator() (Value* value)
// See example in the associated unittest.
template <typename Key, typename Value, typename Deleter,
          typename H = internal::SimpleLRUHash<Key>,
          typename EQ = std::equal_to<Key> >
class SimpleLRUCacheWithDeleter;

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_SIMPLE_LRU_CACHE_H_
