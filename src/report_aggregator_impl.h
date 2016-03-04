// Caches and aggregates report requests.

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_REPORT_AGGREGATOR_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_REPORT_AGGREGATOR_IMPL_H_

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

// Caches/Batches/aggregates report requests and sends them to the server.
// Thread safe.
class ReportAggregatorImpl : public ReportAggregator {
 public:
  // Constructor.
  ReportAggregatorImpl(const std::string& service_name,
                       const ReportAggregationOptions& options,
                       std::shared_ptr<MetricKindMap> metric_kind);

  ~ReportAggregatorImpl() override;

  // Sets the flush callback function.
  void SetFlushCallback(FlushCallback callback);

  // Adds a report request to cache. Returns NOT_FOUND if it could not be
  // aggregated. Callers need to send it to the server.
  ::google::protobuf::util::Status Report(
      const ::google::api::servicecontrol::v1::ReportRequest& request) override;

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  int GetNextFlushInterval() override;

  // Flushes aggregated requests longer than flush_interval.
  // Called at time specified by GetNextFlushInterval().
  ::google::protobuf::util::Status Flush() override;

  // Flushes all cache items. For each item, it will call flush_callback.
  // It is a blocking call, only returns when all items are removed.
  // When calling flush_callback, it is a blocking call too, it will wait for
  // the flush_callback() function return.
  ::google::protobuf::util::Status FlushAll() override;

 private:
  using CacheDeleter = std::function<void(OperationAggregator*)>;
  // Key is the signature of the operation. Value is the
  // OperationAggregator.
  using ReportCache =
    SimpleLRUCacheWithDeleter<std::string, OperationAggregator, CacheDeleter>;

  // Callback function passed to Cache, called when a cache item is removed.
  // Takes ownership of the iop.
  void OnCacheEntryDelete(OperationAggregator* iop);

  // The service name.
  const std::string service_name_;

  ReportAggregationOptions options_;

  // Metric kinds. Key is the metric name and value is the metric kind.
  // Defaults to DELTA if not specified. Not owned.
  std::shared_ptr<MetricKindMap> metric_kinds_;

  // Mutex guarding the access of cache_;
  Mutex cache_mutex_;

  // The cache that maps from operation signature to an operation.
  // We don't calculate fine grained cost for cache entries, assign each
  // entry 1 cost unit.
  // Guarded by mutex_, except when compare against nullptr.
  std::unique_ptr<ReportCache> cache_;

  // Mutex guarding the access of flush_callback_;
  Mutex callback_mutex_;

  // The callback function to flush out cache items.
  ReportAggregator::FlushCallback flush_callback_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ReportAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_REPORT_AGGREGATOR_IMPL_H_
