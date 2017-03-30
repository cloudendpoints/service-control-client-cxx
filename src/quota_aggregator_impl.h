/* Copyright 2017 Google Inc. All Rights Reserved.

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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_QUOTA_AGGREGATOR_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_QUOTA_AGGREGATOR_IMPL_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "google/protobuf/text_format.h"

#include "google/api/metric.pb.h"
#include "google/api/servicecontrol/v1/operation.pb.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "src/aggregator_interface.h"
#include "src/cache_removed_items_handler.h"
#include "src/quota_operation_aggregator.h"
#include "utils/simple_lru_cache.h"
#include "utils/simple_lru_cache_inl.h"
#include "utils/thread.h"

namespace google {
namespace service_control_client {

typedef CacheRemovedItemsHandler<
    ::google::api::servicecontrol::v1::AllocateQuotaRequest>
    AllocateQuotaCacheRemovedItemsHandler;

class QuotaAggregatorImpl : public QuotaAggregator,
                            public AllocateQuotaCacheRemovedItemsHandler {
 public:
  // TODO(jaebong) when an element was expired and aggregated tokens are bigger
  // than 0, aggregator needs to send a request inside the aggregator.
  QuotaAggregatorImpl(const std::string& service_name,
                      const std::string& service_config_id,
                      const QuotaAggregationOptions& options);

  virtual ~QuotaAggregatorImpl();

  // Sets the flush callback function.
  // The callback function must be light and fast.  If it needs to make
  // a remote call, it must be non-blocking call.
  // It should NOT call into this object again from this callback.
  // It will cause dead-lock.
  void SetFlushCallback(FlushCallback callback);

  // If the quota could not be handled by the cache, returns NOT_FOUND,
  // caller has to send the request to service control.
  // Otherwise, returns OK and cached response.
  ::google::protobuf::util::Status Quota(
      const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request,
      ::google::api::servicecontrol::v1::AllocateQuotaResponse* response);

  // Caches a response from a remote Service Controller AllocateQuota call.
  ::google::protobuf::util::Status CacheResponse(
      const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request,
      const ::google::api::servicecontrol::v1::AllocateQuotaResponse& response);

 private:
  class CacheElem {
   public:
    CacheElem(const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
                  response)
        : operation_aggregator_(nullptr),
          quota_response_(response),
          in_flight_(false) {}

    // Aggregates the given request to this cache entry.
    void Aggregate(
        const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request);

    // Returns the aggregated AllocateQuotaRequest and reset the cache entry.
    ::google::api::servicecontrol::v1::AllocateQuotaRequest
    ReturnAllocateQuotaRequestAndClear(const std::string& service_name,
                                       const std::string& service_config_id);

    // Change the negative response to the positive response for refreshing
    void ClearAllocationErrors() { quota_response_.clear_allocate_errors(); }

    // Setter for AllocateQuota response.
    inline void set_quota_response(
        const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
            quota_response) {
      quota_response_ = quota_response;
    }

    // Getter for check response.
    inline const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
    quota_response() const {
      return quota_response_;
    }

    // Return true if aggregated
    inline bool is_aggregated() const {
      return operation_aggregator_ != nullptr;
    }

    // Getter and Setter of signature_
    inline std::string signature() const { return signature_; }
    inline void set_signature(std::string v) { signature_ = v; }

    // Getter and Setter of in_flight_
    inline bool in_flight() const { return in_flight_; }
    inline void set_in_flight(bool v) { in_flight_ = v; }

    inline bool is_positive_response() {
      return quota_response().allocate_errors_size() == 0;
    }

   private:
    // Internal operation.
    std::unique_ptr<QuotaOperationAggregator> operation_aggregator_;

    // The check response for the last check request.
    ::google::api::servicecontrol::v1::AllocateQuotaResponse quota_response_;

    // maintain the sinature to move unnecessary signaure generation
    std::string signature_;

    bool in_flight_;
  };

  using CacheDeleter = std::function<void(CacheElem*)>;

  // Key is the signature of the check request. Value is the CacheElem.
  // It is a LRU cache with MaxIdelTime as response_expiration_time.
  using QuotaCache =
      SimpleLRUCacheWithDeleter<std::string, CacheElem, CacheDeleter>;

  // Methods from: QuotaAggregator interface

  void OnCacheEntryDelete(CacheElem* elem);

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  virtual int GetNextFlushInterval();

  // Invalidates expired allocate quota responses.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush();

  // Flushes out all cached check responses; clears all cache items.
  // Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll();

 private:
  // The service name for this cache.
  const std::string service_name_;
  // The service config id for this cache.
  const std::string service_config_id_;

  // The check aggregation options.
  QuotaAggregationOptions options_;

  // Mutex guarding the access of cache_;
  Mutex cache_mutex_;

  std::unique_ptr<QuotaCache> cache_;
  // flush interval in cycles.
  int64_t flush_interval_in_cycle_;

  bool in_flush_all_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(QuotaAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_QUOTA_AGGREGATOR_IMPL_H_
