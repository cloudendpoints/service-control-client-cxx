#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_INTERFACE_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_INTERFACE_H_

#include <string.h>
#include <memory>
#include <string>

#include "google/api/metric.pb.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {

// Defines a map of a metric name to its metric kind.
typedef std::unordered_map<std::string,
                           ::google::api::MetricDescriptor::MetricKind>
    MetricKindMap;

// Options controlling check aggregation behavior.
struct CheckAggregationOptions {
  // Default constructor.
  CheckAggregationOptions()
      : num_entries(10000), flush_interval_ms(500), expiration_ms(1000) {}

  // Constructor.
  // cache_entries is the maximum number of cache entries that can be kept in
  // the aggregation cache. Cache is disabled when cache_entries <= 0.
  // flush_cache_entry_interval_ms is the maximum milliseconds before an
  // aggregated check request needs to send to remote server again.
  // response_expiration_ms is the maximum milliseconds before a cached check
  // response is invalidated.
  CheckAggregationOptions(int cache_entries, int flush_cache_entry_interval_ms,
                          int response_expiration_ms)
      : num_entries(cache_entries),
        flush_interval_ms(flush_cache_entry_interval_ms),
        expiration_ms(std::max(flush_cache_entry_interval_ms,
                               response_expiration_ms + 1)) {}

  // Maximum number of cache entries kept in the aggregation cache.
  const int num_entries;

  // Maximum milliseconds before aggregated check requests are flushed to the
  // server. The flush is invoked by a check request.
  const int flush_interval_ms;

  // Maximum milliseconds before a cached check response should be deleted. The
  // deletion is invoked by a timer. This value must be larger than
  // flush_interval_ms.
  const int expiration_ms;
};

// Options controlling report aggregation behavior.
struct ReportAggregationOptions {
  // Default constructor.
  ReportAggregationOptions() : num_entries(10000), flush_interval_ms(1000) {}

  // Constructor.
  // cache_entries is the maximum number of cache entries that can be kept in
  // the aggregation cache. Cache is disabled when cache_entries <= 0.
  // flush_cache_entry_interval_ms is the maximum milliseconds before aggregated
  // report requests are flushed to the server. The cache entry is deleted after
  // the flush.
  ReportAggregationOptions(int cache_entries, int flush_cache_entry_interval_ms)
      : num_entries(cache_entries),
        flush_interval_ms(flush_cache_entry_interval_ms) {}

  // Maximum number of cache entries kept in the aggregation cache.
  const int num_entries;

  // Maximum milliseconds before aggregated report requests are flushed to the
  // server. The cache entry is deleted after the flush. The flush is invoked by
  // a timer.
  const int flush_interval_ms;
};

// Aggregate Service_Control Report requests.
// This interface is thread safe.
class ReportAggregator {
 public:
  // Flush callback can be called when calling any of member functions.
  // If the callback function is blocked, the called member function, such as
  // Report(), will be blocked too. It is recommended that the callback function
  // should be fast and non blocking.
  using FlushCallback = std::function<void(
      const ::google::api::servicecontrol::v1::ReportRequest&)>;

  virtual ~ReportAggregator() {}

  // Sets the flush callback function.
  // The callback function must be light and fast.  If it needs to make
  // a remote call, it must be non-blocking call.
  // It should NOT call into this object again from this callback.
  // It will cause dead-lock.
  virtual void SetFlushCallback(FlushCallback callback) = 0;

  // Adds a report request to cache
  virtual ::google::protobuf::util::Status Report(
      const ::google::api::servicecontrol::v1::ReportRequest& request) = 0;

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  virtual int GetNextFlushInterval() = 0;

  // Flushes aggregated requests longer than flush_interval.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush() = 0;

  // Flushes out aggregated report requests, clears all cache items.
  // Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll() = 0;

 protected:
  ReportAggregator() {}
};

// Aggregate Service_Control Check requests.
// This interface is thread safe.
class CheckAggregator {
 public:
  // Flush callback can be called when calling any of member functions.
  // If the callback function is blocked, the called member function, such as
  // Check(), will be blocked too. It is recommended that the callback function
  // should be fast and non blocking.
  using FlushCallback = std::function<void(
      const ::google::api::servicecontrol::v1::CheckRequest&)>;

  virtual ~CheckAggregator() {}

  // Sets the flush callback function.
  // The callback function must be light and fast.  If it needs to make
  // a remote call, it must be non-blocking call.
  // It should NOT call into this object again from this callback.
  // It will cause dead-lock.
  virtual void SetFlushCallback(FlushCallback callback) = 0;

  // If the check could not be handled by the cache, returns NOT_FOUND,
  // caller has to send the request to service control.
  // Otherwise, returns OK and cached response.
  virtual ::google::protobuf::util::Status Check(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      ::google::api::servicecontrol::v1::CheckResponse* response) = 0;

  // Caches a response from a remote Service Controller Check call.
  virtual ::google::protobuf::util::Status CacheResponse(
      const ::google::api::servicecontrol::v1::CheckRequest& request,
      const ::google::api::servicecontrol::v1::CheckResponse& response) = 0;

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  virtual int GetNextFlushInterval() = 0;

  // Invalidates expired check resposnes.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush() = 0;

  // Flushes out all cached check responses; clears all cache items.
  // Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll() = 0;

 protected:
  CheckAggregator() {}
};

// Creates a report aggregator.
std::unique_ptr<ReportAggregator> CreateReportAggregator(
    const std::string& service_name, const ReportAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kind);

// Creates a check aggregator.
std::unique_ptr<CheckAggregator> CreateCheckAggregator(
    const std::string& service_name, const CheckAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kind);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_INTERFACE_H_
