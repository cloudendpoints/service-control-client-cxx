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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_OPTIONS_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_OPTIONS_H_

#include <memory>
#include "google/api/metric.pb.h"

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
  // response is invalidated. We make sure that it is at least
  // flush_cache_entry_interval_ms + 1.
  CheckAggregationOptions(int cache_entries, int flush_cache_entry_interval_ms,
                          int response_expiration_ms)
      : num_entries(cache_entries),
        flush_interval_ms(flush_cache_entry_interval_ms),
        expiration_ms(std::max(flush_cache_entry_interval_ms + 1,
                               response_expiration_ms)) {}

  // Maximum number of cache entries kept in the aggregation cache.
  // Set to 0 will disable caching and aggregation.
  const int num_entries;

  // Maximum milliseconds before aggregated check requests are flushed to the
  // server. The flush is triggered by a check request.
  const int flush_interval_ms;

  // Maximum milliseconds before a cached check response should be deleted. The
  // deletion is triggered by a timer. This value must be larger than
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
  // Set to 0 will disable caching and aggregation.
  const int num_entries;

  // Maximum milliseconds before aggregated report requests are flushed to the
  // server. The flush is triggered by a timer.
  const int flush_interval_ms;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_OPTIONS_H_
