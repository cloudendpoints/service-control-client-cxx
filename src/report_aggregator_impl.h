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
#include "src/cache_removed_items_handler.h"
#include "src/operation_aggregator.h"
#include "utils/simple_lru_cache.h"
#include "utils/simple_lru_cache_inl.h"
#include "utils/thread.h"

namespace google {
namespace service_control_client {

// Caches/Batches/aggregates report requests and sends them to the server.
// Thread safe.
typedef CacheRemovedItemsHandler<
    ::google::api::servicecontrol::v1::ReportRequest>
    ReportCacheRemovedItemsHandler;

class ReportAggregatorImpl : public ReportAggregator,
                             public ReportCacheRemovedItemsHandler {
 public:
  // Constructor.
  ReportAggregatorImpl(const std::string& service_name,
                       const std::string& service_config_id,
                       const ReportAggregationOptions& options,
                       std::shared_ptr<MetricKindMap> metric_kind);

  virtual ~ReportAggregatorImpl();

  // Sets the flush callback function.
  virtual void SetFlushCallback(FlushCallback callback);

  // Adds a report request to cache. Returns NOT_FOUND if it could not be
  // aggregated. Callers need to send it to the server.
  virtual ::google::protobuf::util::Status Report(
      const ::google::api::servicecontrol::v1::ReportRequest& request);

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  virtual int GetNextFlushInterval();

  // Flushes aggregated requests longer than flush_interval.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush();

  // Flushes all cache items. For each item, it will call flush_callback.
  // It is a blocking call, only returns when all items are removed.
  // When calling flush_callback, it is a blocking call too, it will wait for
  // the flush_callback() function return.
  virtual ::google::protobuf::util::Status FlushAll();

 private:
  using CacheDeleter = std::function<void(OperationAggregator*)>;
  // Key is the signature of the operation. Value is the
  // OperationAggregator.
  using ReportCache =
      SimpleLRUCacheWithDeleter<std::string, OperationAggregator, CacheDeleter>;

  // Callback function passed to Cache, called when a cache item is removed.
  // Takes ownership of the iop.
  void OnCacheEntryDelete(OperationAggregator* iop);

  // Tries to merge two report requests.
  bool MergeItem(
      const ::google::api::servicecontrol::v1::ReportRequest& new_item,
      ::google::api::servicecontrol::v1::ReportRequest* old_item);

  // The service name.
  const std::string service_name_;
  // The service config id.
  const std::string service_config_id_;

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

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ReportAggregatorImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_REPORT_AGGREGATOR_IMPL_H_
