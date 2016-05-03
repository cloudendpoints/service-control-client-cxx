/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_CACHE_REMOVED_ITEMS_HANDLER_H
#define GOOGLE_SERVICE_CONTROL_CLIENT_CACHE_REMOVED_ITEMS_HANDLER_H

#include "src/aggregator_interface.h"
#include "utils/simple_lru_cache.h"
#include "utils/simple_lru_cache_inl.h"
#include "utils/thread.h"

namespace google {
namespace service_control_client {
// If flush callback is called inside OnCacheEntryDelete(), the callback
// function SHOULD not call any of AggregatorImpl functions, otherwise it
// will cause deadlock. The reason is: OnCacheEntryDelete() is already holding
// cache_mutex_ lock and all of CheckAggregator methods will try acquire
// cache_mutex_ lock which will cause dead lock.
// The solution is to buffer the removed items into a stack allocated vector
// in OnCacheEntryDelete(). After cache_mutex lock is released, calls flush
// callback for these removed items.
// class CacheRemovedItemsHandler is designed to implement this solution.
// Both CheckAggregator and ReportAggregator are derived from
// CacheRemovedItemsHandler. CacheRemovedItemsHandler has a member variable
// stack_buffer_ to point to stack allocated vector which can be used to insert
// cache removed item in OnCacheEntryDelete(). Actual vector has to be allocated
// from stack by each caller of cache operation functions. Swapper can be
// used to set the vector pointer and reset it. Here is a typical usage of
// this class:
//    ReportCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
//    MutexLock lock(cache_mutex_);
//    ReportCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
//                                                                 &stack_buffer);
// cache_mutex_ lock has to be in between of the instantiation of StackBuffer
// and the instantiation ofSwapper. All cache operations (which may evict cache
// items) need to be wrapped by this code pattern.
template <class RequestType>
class CacheRemovedItemsHandler {
 public:
  CacheRemovedItemsHandler() : flush_callback_(NULL), stack_buffer_(NULL) {}

  virtual ~CacheRemovedItemsHandler() {}

 protected:
  // The callback function to flush out cache items.
  using InternalFlushCallback = std::function<void(const RequestType&)>;

  // Sets the flush callback function.
  void InternalSetFlushCallback(InternalFlushCallback callback) {
    MutexLock lock(callback_mutex_);
    flush_callback_ = callback;
  }

  void AddRemovedItem(const RequestType& item) {
    if (stack_buffer_) {
      stack_buffer_->Add(item);
    }
  }

  // Checks if an new item can be merged into an old item.
  // Derived class will implement this: CheckRequest will never merge.
  // A ReportRequest can carry multiple operations, it can merge many
  // reuqests until number of operations reaches certain size.
  virtual bool MergeItem(const RequestType& new_item, RequestType* old_item) {
    return false;
  }

  // Class StackBuffer is designed to maintain the stack allocated vector which
  // can be used to insert cache removed items. This class has to be
  // instantiated at stack. It should be used outside of cache_mutex lock.
  class StackBuffer final {
   public:
    StackBuffer(CacheRemovedItemsHandler* handler) : handler_(handler) {}

    virtual ~StackBuffer() {
      for (const auto& request : items_) {
        handler_->FlushOut(request);
      }
    }

    void Add(const RequestType& item) {
      if (items_.empty() ||
          !handler_->MergeItem(item, &items_[items_.size() - 1])) {
        items_.push_back(item);
      }
    }

    // Class Swapper is used to swap stack_buffer_ variable in the
    // CacheRemovedItemsHandle class. It should be used within cache_mutex lock.
    class Swapper final {
     public:
      Swapper(CacheRemovedItemsHandler* handler, StackBuffer* buffer)
          : handler_(handler) {
        handler_->stack_buffer_ = buffer;
      }

      virtual ~Swapper() { handler_->stack_buffer_ = NULL; }

     private:
      CacheRemovedItemsHandler* handler_;
    };

   private:
    CacheRemovedItemsHandler* handler_;
    // A vector to cache store removed items.
    std::vector<RequestType> items_;
  };

 private:
  // Mutex guarding the access of flush_callback_;
  Mutex callback_mutex_;

  // The callback function to flush out cache items.
  InternalFlushCallback flush_callback_;

  // The pointer points to the StackBuffer instance where allocated vector is
  // stored. It should only be set and reset by StackBuffer::Swapper.
  StackBuffer* stack_buffer_;

  void FlushOut(const RequestType& request) {
    MutexLock lock(callback_mutex_);
    if (flush_callback_) {
      flush_callback_(request);
    }
  }
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_CACHE_REMOVED_ITEMS_HANDLER_H
