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

// Caches and aggregates check requests.

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_CHECK_AGGREGATOR_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_CHECK_AGGREGATOR_IMPL_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "google/api/metric.pb.h"
#include "google/api/servicecontrol/v1/operation.pb.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "src/aggregator_interface.h"
#include "src/cache_removed_items_handler.h"
#include "src/operation_aggregator.h"
#include "utils/simple_lru_cache.h"
#include "utils/simple_lru_cache_inl.h"
#include "utils/thread.h"

namespace google {
namespace service_control_client {

// Caches/Batches/aggregates check requests and sends them to the server.
// Thread safe.
//
// Some typical data flows:
//
// Creates a new cache entry and use cached response:
// 1) Calls Check(), and it returns NOT_FOUND,
// 2) Callers send the request to server and get its response.
// 3) Calls CacheResponse() to set the response to cache.
// 4) The new Check() calls after will find the cached response, use it.
// 5) If the request has quota info, it will be aggregated to the cached entry.
//
// Refreshes a cached entry after refresh interval:
// 1) Calls Check(), found a cached response,
// 2) If it passes refresh_interval, Check() returns NOT_FOUND, callers
//    will send the request to server.
// 3) The new Check() calls after will use old response before the new response
//      arrives.
// 4) Callers will set the new response by calling CacheResponse().
// 5) The new Check() calls after will use the new response.
//
// After a response is expired:
// 1) During Flush() call, if a cached response is expired, it wil be flushed
//    out.  If it has aggregated quota info, flush_callback will be called to
//    send the request to server.
// 2) Callers need to send the request to server, and get its new response.
// 3) The new response will be added to the cache by calling CacheResponse().
// 4) If there is not Check() called for that entry before it is expired again,
//    it will NOT have aggregated data when that entry is removed. It will not
//    send to flush_callback(). The item simply just got deleted.
//
// Object life management:
// The callers of this object needs to make sure the object is still valid
// before calling its methods. Specifically, callers may use async
// transport to send request to server and pass an on_done() callback to be
// called when response is received.  If on_done() function is calling
// CheckAggregator->CacheReponse() funtion, caller MUST make sure the
// CacheAggregator object is still valid.

typedef CacheRemovedItemsHandler<
    ::google::api::servicecontrol::v1::CheckRequest>
    CheckCacheRemovedItemsHandler;

class CheckAggregatorImpl : public CheckAggregator,
                            public CheckCacheRemovedItemsHandler {
 public:
  // Constructor.
  // Does not take ownership of metric_kinds and controller, which must outlive
  // this instance.
  CheckAggregatorImpl(const std::string& service_name,
                      const CheckAggregationOptions& options,
                      std::shared_ptr<MetricKindMap> metric_kind);

  ~CheckAggregatorImpl() override;

  // Sets the flush callback function.
  // It is called when a cache entry is expired and it has aggregated quota
  // in the request. The callback function needs to send the request to server,
  // calls CacheResponse() to set its response.
  void SetFlushCallback(FlushCallback callback) override;

  // If the check could not be handled by the cache, returns NOT_FOUND,
  // caller has to send the request to service control server and call
  // CacheResponse() to set the response to the cache.
  // Otherwise, returns OK and cached response.
  ::google::protobuf::util::Status Check(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      ::google::api::servicecontrol::v1::CheckResponse* response) override;

  // Caches a response from a remote Service Controller Check call.
  ::google::protobuf::util::Status CacheResponse(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      const ::google::api::servicecontrol::v1::CheckResponse& response)
      override;

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  int GetNextFlushInterval() override;

  // Flushes expired cache response entries.
  // Called at time specified by GetNextFlushInterval().
  ::google::protobuf::util::Status Flush() override;

  // Flushes out all cache items. Usually called at destructor.
  ::google::protobuf::util::Status FlushAll() override;

 private:
  // Cache entry for aggregated check requests and previous check response.
  class CacheElem {
   public:
    CacheElem(const ::google::api::servicecontrol::v1::CheckResponse& response,
              const int64_t time, const int quota_scale)
        : check_response_(response),
          last_check_time_(time),
          quota_scale_(quota_scale),
          is_flushing_(false) {}

    // Aggregates the given request to this cache entry.
    void Aggregate(
        const ::google::api::servicecontrol::v1::CheckRequest& request,
        const MetricKindMap* metric_kinds);

    // Returns the aggregated CheckRequest and reset the cache entry.
    ::google::api::servicecontrol::v1::CheckRequest ReturnCheckRequestAndClear(
        const std::string& service_name);

    bool HasPendingCheckRequest() const {
      return operation_aggregator_ != NULL;
    }

    // Setter for check response.
    inline void set_check_response(
        const ::google::api::servicecontrol::v1::CheckResponse&
            check_response) {
      check_response_ = check_response;
    }
    // Getter for check response.
    inline const ::google::api::servicecontrol::v1::CheckResponse&
    check_response() const {
      return check_response_;
    }

    // Setter for last check time.
    inline void set_last_check_time(const int64_t last_check_time) {
      last_check_time_ = last_check_time;
    }
    // Getter for last check time.
    inline const int64_t last_check_time() const { return last_check_time_; }

    // Setter for check response.
    inline void set_quota_scale(const int quota_scale) {
      quota_scale_ = quota_scale;
    }
    // Getter for check response.
    inline int quota_scale() const { return quota_scale_; }

    // Getter and Setter of is_flushing_;
    inline bool is_flushing() const { return is_flushing_; }
    inline void set_is_flushing(bool v) { is_flushing_ = v; }

   private:
    // Internal operation.
    std::unique_ptr<OperationAggregator> operation_aggregator_;

    // The check response for the last check request.
    ::google::api::servicecontrol::v1::CheckResponse check_response_;
    // In general, this is the last time a check response is updated.
    //
    // During flush, we set it to be the request start time to prevent a next
    // check request from triggering another flush. Note that this prevention
    // works only during the flush interval, which means for long RPC, there
    // could be up to RPC_time/flush_interval ongoing check requests.
    int64_t last_check_time_;
    // Scale used to predict how much quota are charged. It is calculated
    // as the tokens charged in the last check response / requested tokens.
    // The predicated amount tokens consumed is then request tokens * scale.
    // This field is valid only when check_response has no check errors.
    int quota_scale_;

    // If true, is sending the request to server to get new response.
    bool is_flushing_;
  };

  using CacheDeleter = std::function<void(CacheElem*)>;
  // Key is the signature of the check request. Value is the CacheElem.
  // It is a LRU cache with MaxIdelTime as response_expiration_time.
  using CheckCache =
      SimpleLRUCacheWithDeleter<std::string, CacheElem, CacheDeleter>;

  // Returns whether we should flush a cache entry.
  //   If the aggregated check request is less than flush interval, no need to
  //   flush.
  bool ShouldFlush(const CacheElem& elem);

  // Flushes the internal operation in the elem and delete the elem. The
  // response from the server is NOT cached.
  // Takes ownership of the elem.
  void OnCacheEntryDelete(CacheElem* elem);

  // The service name for this cache.
  const std::string service_name_;

  // The check aggregation options.
  CheckAggregationOptions options_;

  // Metric kinds. Key is the metric name and value is the metric kind.
  // Defaults to DELTA if not specified. Not owned.
  std::shared_ptr<MetricKindMap> metric_kinds_;

  // Mutex guarding the access of cache_;
  Mutex cache_mutex_;

  // The cache that maps from operation signature to an operation.
  // We don't calculate fine grained cost for cache entries, assign each
  // entry 1 cost unit.
  // Guarded by mutex_, except when compare against NULL.
  std::unique_ptr<CheckCache> cache_;

  // flush interval in cycles.
  int64_t flush_interval_in_cycle_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CheckAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_CHECK_AGGREGATOR_IMPL_H_
