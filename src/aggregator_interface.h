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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_INTERFACE_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_AGGREGATOR_INTERFACE_H_

#include <string.h>
#include <memory>
#include <string>

#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"
#include "include/aggregation_options.h"

namespace google {
namespace service_control_client {

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
