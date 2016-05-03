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

#include "src/report_aggregator_impl.h"
#include "src/signature.h"

#include "google/protobuf/stubs/logging.h"

using std::string;
using ::google::api::MetricDescriptor;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

// A report can carry many operations, merge multiple report requests
// into one report request until its number of operations reach this limit.
const int kMaxOperationsToSend = 1000;

// Returns whether the given report request has high value operations.
bool HasHighImportantOperation(const ReportRequest& request) {
  for (const auto& operation : request.operations()) {
    if (operation.importance() != Operation::LOW) {
      return true;
    }
  }
  return false;
}

}  // namespace

ReportAggregatorImpl::ReportAggregatorImpl(
    const string& service_name, const ReportAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kinds)
    : service_name_(service_name),
      options_(options),
      metric_kinds_(metric_kinds) {
  if (options.num_entries > 0) {
    cache_.reset(
        new ReportCache(options.num_entries,
                        std::bind(&ReportAggregatorImpl::OnCacheEntryDelete,
                                  this, std::placeholders::_1)));
    cache_->SetAgeBasedEviction(options.flush_interval_ms / 1000.0);
  }
}

ReportAggregatorImpl::~ReportAggregatorImpl() {
  // FlushAll() is a blocking call to remove all cache items.
  // For each removed item, it will call flush_callback().
  // At the destructor, it is better not to call the callback.
  SetFlushCallback(NULL);
  FlushAll();
}

// Set the flush callback function.
void ReportAggregatorImpl::SetFlushCallback(FlushCallback callback) {
  InternalSetFlushCallback(callback);
}

// Add a report request to cache
Status ReportAggregatorImpl::Report(
    const ::google::api::servicecontrol::v1::ReportRequest& request) {
  if (request.service_name() != service_name_) {
    return Status(Code::INVALID_ARGUMENT,
                  (string("Invalid service name: ") + request.service_name() +
                   string(" Expecting: ") + service_name_));
  }
  if (HasHighImportantOperation(request) || !cache_) {
    // By returning NO_FOUND, caller will send request to server.
    return Status(Code::NOT_FOUND, "");
  }

  ReportCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  ReportCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                               &stack_buffer);

  // Starts to cache and aggregate low important operations.
  for (const auto& operation : request.operations()) {
    string signature = GenerateReportOperationSignature(operation);

    ReportCache::ScopedLookup lookup(cache_.get(), signature);
    if (lookup.Found()) {
      lookup.value()->MergeOperation(operation);
    } else {
      OperationAggregator* iop =
          new OperationAggregator(operation, metric_kinds_.get());
      cache_->Insert(signature, iop, 1);
    }
  }
  return Status::OK;
}

void ReportAggregatorImpl::OnCacheEntryDelete(OperationAggregator* iop) {
  // iop or cache is under projected.  This function is only called when
  // cache::Insert() or cache::Removed() is called and these operations
  // are already protected by cache_mutex.
  ReportRequest request;
  request.set_service_name(service_name_);
  // TODO(qiwzhang): Remove this copy
  *(request.add_operations()) = iop->ToOperationProto();
  delete iop;

  AddRemovedItem(request);
}

bool ReportAggregatorImpl::MergeItem(const ReportRequest& new_item,
                                     ReportRequest* old_item) {
  if (old_item->service_name() != new_item.service_name() ||
      old_item->operations().size() + new_item.operations().size() >
          kMaxOperationsToSend) {
    return false;
  }
  old_item->MergeFrom(new_item);
  return true;
}

// When the next Flush() should be called.
// Return in ms from now, or -1 for never
int ReportAggregatorImpl::GetNextFlushInterval() {
  if (!cache_) return -1;
  return options_.flush_interval_ms;
}

// Flush aggregated requests whom are longer than flush_interval.
// Called at time specified by GetNextFlushInterval().
Status ReportAggregatorImpl::Flush() {
  ReportCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  ReportCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                               &stack_buffer);
  if (cache_) {
    cache_->RemoveExpiredEntries();
  }
  return Status::OK;
}

// Flush out aggregated report requests, clear all cache items.
// Usually called at destructor.
Status ReportAggregatorImpl::FlushAll() {
  ReportCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  ReportCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                               &stack_buffer);
  GOOGLE_LOG(INFO) << "Remove all entries of report aggregator.";
  if (cache_) {
    cache_->RemoveAll();
  }
  return Status::OK;
}

std::unique_ptr<ReportAggregator> CreateReportAggregator(
    const std::string& service_name, const ReportAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kind) {
  return std::unique_ptr<ReportAggregator>(
      new ReportAggregatorImpl(service_name, options, metric_kind));
}

}  // namespace service_control_client
}  // namespace google
