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
#include "src/operation_aggregator.h"
#include "utils/simple_lru_cache.h"
#include "utils/simple_lru_cache_inl.h"
#include "utils/thread.h"

namespace google {
namespace service_control_client {

// Caches/Batches/aggregates check requests and sends them to the server.
// Thread safe.
class CheckAggregatorImpl : public CheckAggregator {
 public:
  // Constructor.
  // Does not take ownership of metric_kinds and controller, which must outlive
  // this instance.
  CheckAggregatorImpl(const std::string& service_name,
                      const CheckAggregationOptions& options,
                      std::shared_ptr<MetricKindMap> metric_kind);

  virtual ~CheckAggregatorImpl();

  // Set the flush callback function.
  // It is called when a cache entry is expired and it has aggregated quota
  // in the request needs to be send out to server.
  // If quota is not supported, this will never be called.
  void SetFlushCallback(FlushCallback callback);

  // If the check could not be handled by the cache, return NOT_FOUND,
  // caller has to send the request to service control.
  // Otherwise, return OK and cached response.
  ::google::protobuf::util::Status Check(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      ::google::api::servicecontrol::v1::CheckResponse* response);

  // Cache a response from a remote Service Controller Check call.
  ::google::protobuf::util::Status CacheResponse(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      const ::google::api::servicecontrol::v1::CheckResponse& response);

  // When the next Flush() should be called.
  // Return in ms from now, or -1 for never
  int NextFlushInterval();

  // Flush response expired cache entries.
  // Called at time specified by NextFlushInterval().
  ::google::protobuf::util::Status Flush();

  // Flush out aggregated check requests, clear all cache items.
  // Usually called at destructor.
  ::google::protobuf::util::Status FlushAll();

 private:
  // Cache entry for aggregated check requests and previous check response.
  class CacheElem {
   public:
    CacheElem(const ::google::api::servicecontrol::v1::CheckResponse& response,
              const int64_t time, const int quota_scale)
        : check_response_(response),
          last_check_time_(time),
          quota_scale_(quota_scale) {}

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

  const std::string service_name_;

  CheckAggregationOptions options_;

  // Metric kinds. Key is the metric name and value is the metric kind.
  // Defaults to DELTA if not specified. Not owned.
  std::shared_ptr<MetricKindMap> metric_kinds_;

  // Mutex guarding the access of cache_;
  mutable Mutex mutex_;

  // The cache that maps from operation signature to an operation.
  // We don't calculate fine grained cost for cache entries, assign each
  // entry 1 cost unit.
  // Guarded by mutex_, except when compare against NULL.
  std::unique_ptr<CheckCache> cache_;

  // The callback function to flush out cache items.
  CheckAggregator::FlushCallback flush_callback_;

  int64_t flush_interval_in_cycle_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CheckAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_CHECK_AGGREGATOR_IMPL_H_
