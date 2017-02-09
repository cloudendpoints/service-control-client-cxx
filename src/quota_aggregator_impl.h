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

#ifndef BAZEL_ESP_EXTERNAL_SERVICECONTROL_CLIENT_GIT_SRC_QUOTA_AGGREGATOR_IMPL_H_
#define BAZEL_ESP_EXTERNAL_SERVICECONTROL_CLIENT_GIT_SRC_QUOTA_AGGREGATOR_IMPL_H_

#include <string>
#include <unordered_map>
#include <utility>

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
                      const QuotaAggregationOptions& options,
                      std::shared_ptr<MetricKindMap> metric_kinds);

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
  // void AddWait
  // TODO(jaebong):: add method

  class CacheElem {
   public:
    CacheElem(const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
                  response,
              const int64_t time)
        : operation_aggregator_(nullptr),
          quota_response_(response),
          last_check_time_(time),
          is_refreshing_(false),
          is_aggregated_(false) {}

    // Aggregates the given request to this cache entry.
    void Aggregate(
        const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request,
        const MetricKindMap* metric_kinds);

    // Returns the aggregated AllocateQuotaRequest and reset the cache entry.
    ::google::api::servicecontrol::v1::AllocateQuotaRequest
    ReturnAllocateQuotaRequestAndClear(const std::string& service_name,
                                       const std::string& service_config_id);

    bool HasPendingAllocateQuotaRequest() const {
      return operation_aggregator_ != NULL;
    }

    // Setter for AllocateQuota response.
    inline void set_quota_response(
        const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
            quota_response) {
      quota_response_ = quota_response;
    }
    // Getter for check response.
    inline const ::google::api::servicecontrol::v1::AllocateQuotaResponse&
    check_response() const {
      return quota_response_;
    }

    // Setter for last check time.
    inline void set_last_check_time(const int64_t last_check_time) {
      last_check_time_ = last_check_time;
    }
    // Getter for last check time.
    inline const int64_t last_check_time() const { return last_check_time_; }

    // Getter and Setter of is_refreshing_;
    inline bool is_refreshing() const { return is_refreshing_; }
    inline void set_is_refreshing(bool v) { is_refreshing_ = v; }

    // Getter and Setter of is_aggregated_
    inline bool is_aggregated() const { return is_aggregated_; }
    inline void set_is_aggregated(bool v) { is_aggregated_ = v; }

    // Getter and Setter of signature_
    inline std::string signature() const { return signature_; }
    inline void set_signature(std::string v) { signature_ = v; }

    inline std::unordered_map<std::string, int64_t>
    quota_metrics_aggregation() {
      return quota_metrics_aggregation_;
    }

    inline void clear_quota_metrics_aggregation() {
      quota_metrics_aggregation_.clear();
      is_aggregated_ = false;
    }

   private:
    // Internal operation.
    std::unique_ptr<QuotaOperationAggregator> operation_aggregator_;

    // TODO(jae):: cache aggregation?
    std::unordered_map<std::string, int64_t> quota_metrics_aggregation_;

    // The check response for the last check request.
    ::google::api::servicecontrol::v1::AllocateQuotaResponse quota_response_;

    // In general, this is the last time a check response is updated.
    //
    // During flush, we set it to be the request start time to prevent a next
    // check request from triggering another flush. Note that this prevention
    // works only during the flush interval, which means for long RPC, there
    // could be up to RPC_time/flush_interval ongoing check requests.
    int64_t last_check_time_;

    // If true, is sending the request to server to get new response.
    bool is_refreshing_;

    bool is_aggregated_;

    std::string signature_;
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

  // Invalidates expired allocate quota resposnes.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush();

  // Flushes out all cached check responses; clears all cache items.
  // Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll();

  // Methods from CacheRemovedItemsHandler

 private:
  // The service name for this cache.
  const std::string service_name_;
  // The service config id for this cache.
  const std::string service_config_id_;

  // The check aggregation options.
  QuotaAggregationOptions options_;

  // Metric kinds. Key is the metric name and value is the metric kind.
  // Defaults to DELTA if not specified. Not owned.
  std::shared_ptr<MetricKindMap> metric_kinds_;

  // Mutex guarding the access of cache_;
  Mutex cache_mutex_;

  std::unique_ptr<QuotaCache> cache_;
  // flush interval in cycles.
  int64_t flush_interval_in_cycle_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(QuotaAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // BAZEL_ESP_EXTERNAL_SERVICECONTROL_CLIENT_GIT_SRC_QUOTA_AGGREGATOR_IMPL_H_
