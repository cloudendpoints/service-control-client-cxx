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

#include "src/check_aggregator_impl.h"
#include "src/signature.h"

#include "google/protobuf/stubs/logging.h"

using std::string;
using ::google::api::MetricDescriptor;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::google::service_control_client::SimpleCycleTimer;

namespace google {
namespace service_control_client {

void CheckAggregatorImpl::CacheElem::Aggregate(
    const CheckRequest& request, const MetricKindMap* metric_kinds) {
  if (operation_aggregator_ == NULL) {
    operation_aggregator_.reset(
        new OperationAggregator(request.operation(), metric_kinds));
  } else {
    operation_aggregator_->MergeOperation(request.operation());
  }
}

CheckRequest CheckAggregatorImpl::CacheElem::ReturnCheckRequestAndClear(
    const string& service_name) {
  CheckRequest request;
  request.set_service_name(service_name);

  if (operation_aggregator_ != NULL) {
    *(request.mutable_operation()) = operation_aggregator_->ToOperationProto();
    operation_aggregator_ = NULL;
  }
  return request;
}

CheckAggregatorImpl::CheckAggregatorImpl(
    const string& service_name, const CheckAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kinds)
    : service_name_(service_name),
      options_(options),
      metric_kinds_(metric_kinds) {
  // Converts flush_interval_ms to Cycle used by SimpleCycleTimer.
  flush_interval_in_cycle_ =
      options_.flush_interval_ms * SimpleCycleTimer::Frequency() / 1000;

  if (options.num_entries > 0) {
    cache_.reset(new CheckCache(
        options.num_entries, std::bind(&CheckAggregatorImpl::OnCacheEntryDelete,
                                       this, std::placeholders::_1)));
    cache_->SetMaxIdleSeconds(options.expiration_ms / 1000.0);
  }
}

CheckAggregatorImpl::~CheckAggregatorImpl() {
  // FlushAll() will remove all cache items. For each removed item, it will call
  // flush_callback.  At destructor, it is better not to call the callback.
  SetFlushCallback(NULL);
  FlushAll();
}

// Set the flush callback function.
void CheckAggregatorImpl::SetFlushCallback(FlushCallback callback) {
  InternalSetFlushCallback(callback);
}

// Add a check request to cache
Status CheckAggregatorImpl::Check(const CheckRequest& request,
                                  CheckResponse* response) {
  if (request.service_name() != service_name_) {
    return Status(Code::INVALID_ARGUMENT,
                  (string("Invalid service name: ") + request.service_name() +
                   string(" Expecting: ") + service_name_));
  }
  if (!request.has_operation()) {
    return Status(Code::INVALID_ARGUMENT, "operation field is required.");
  }
  if (request.operation().importance() != Operation::LOW || !cache_) {
    // By returning NO_FOUND, caller will send request to server.
    return Status(Code::NOT_FOUND, "");
  }

  string request_signature = GenerateCheckRequestSignature(request);

  CheckCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  CheckCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                              &stack_buffer);

  CheckCache::ScopedLookup lookup(cache_.get(), request_signature);
  if (!lookup.Found()) {
    // By returning NO_FOUND, caller will send request to server.
    return Status(Code::NOT_FOUND, "");
  }

  CacheElem* elem = lookup.value();

  // If the cached check response has check errors, then we assume the new
  // request should fail as well and return the cached check response.
  // However, after the flush interval, the first check request will be send to
  // the server to refresh the check response. Other check requests still fail
  // with cached check response.
  // If the cached check response is a pass, then we assume the new request
  // should pass as well and return the cached response directly, besides
  // updating the quota info to be the same as requested. The requested tokens
  // are aggregated until flushed.
  // More details can be found in design doc go/simple-chemist-client.
  if (elem->check_response().check_errors_size() > 0) {
    if (ShouldFlush(*elem)) {
      // Pretend that we did not find, so we can force it into a check request
      // to the server.
      //
      // Setting last check to now to block more check requests to Chemist.
      elem->set_last_check_time(SimpleCycleTimer::Now());
      // By returning NO_FOUND, caller will send request to server.
      return Status(Code::NOT_FOUND, "");
    } else {
      // Use cached response.
      *response = elem->check_response();
      return Status::OK;
    }
  } else {
    elem->Aggregate(request, metric_kinds_.get());

    if (ShouldFlush(*elem)) {
      if (elem->is_flushing()) {
        GOOGLE_LOG(WARNING) << "Last refresh request was not completed yet.";
      }
      elem->set_is_flushing(true);
      // Setting last check to now to block more check requests to Chemist.
      elem->set_last_check_time(SimpleCycleTimer::Now());
      // By returning NO_FOUND, caller will send request to server.
      return Status(Code::NOT_FOUND, "");
    }

    *response = elem->check_response();
  }
  // TODO(qiwzhang): supports quota
  // ScaleQuotaTokens(request, elem->quota_scale(), response);

  return Status::OK;
}

bool CheckAggregatorImpl::ShouldFlush(const CacheElem& elem) {
  int64_t age = SimpleCycleTimer::Now() - elem.last_check_time();
  // TODO(chengliang): consider accumulated tokens as well. If the
  // accumulated number of tokens is larger than a threshold, we may also
  // decide to send the request.
  //
  // This will prevent sending more RPCs while there is an ongoing one most of
  // the time, except when there is a long RPC that exceeds the flush interval.
  return age >= flush_interval_in_cycle_;
}

Status CheckAggregatorImpl::CacheResponse(const CheckRequest& request,
                                          const CheckResponse& response) {
  CheckCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  CheckCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                              &stack_buffer);
  if (cache_) {
    string request_signature = GenerateCheckRequestSignature(request);
    CheckCache::ScopedLookup lookup(cache_.get(), request_signature);

    int64_t now = SimpleCycleTimer::Now();
    // TODO(qiwzhang): supports quota
    // int scale = GetQuotaScale(request, response);
    int quota_scale = 0;
    if (lookup.Found()) {
      lookup.value()->set_last_check_time(now);
      lookup.value()->set_check_response(response);
      lookup.value()->set_quota_scale(quota_scale);
      lookup.value()->set_is_flushing(false);
    } else {
      CacheElem* cache_elem = new CacheElem(response, now, quota_scale);
      cache_->Insert(request_signature, cache_elem, 1);
    }
  }

  return Status::OK;
}

// When the next Flush() should be called.
// Flush() call remove expired response.
int CheckAggregatorImpl::GetNextFlushInterval() {
  if (!cache_) return -1;
  return options_.expiration_ms;
}

// Flush aggregated requests whom are longer than flush_interval.
// Called at time specified by GetNextFlushInterval().
Status CheckAggregatorImpl::Flush() {
  CheckCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  CheckCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                              &stack_buffer);
  if (cache_) {
    cache_->RemoveExpiredEntries();
  }

  return Status::OK;
}

void CheckAggregatorImpl::OnCacheEntryDelete(CacheElem* elem) {
  if (!elem->HasPendingCheckRequest()) {
    delete elem;
    return;
  }

  CheckRequest request;
  request = elem->ReturnCheckRequestAndClear(service_name_);
  AddRemovedItem(request);
  delete elem;
}

// Flush out aggregated check requests, clear all cache items.
// Usually called at destructor.
Status CheckAggregatorImpl::FlushAll() {
  CheckCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  CheckCacheRemovedItemsHandler::StackBuffer::Swapper swapper(this,
                                                              &stack_buffer);
  GOOGLE_LOG(INFO) << "Remove all entries of check aggregator.";
  if (cache_) {
    cache_->RemoveAll();
  }

  return Status::OK;
}

std::unique_ptr<CheckAggregator> CreateCheckAggregator(
    const std::string& service_name, const CheckAggregationOptions& options,
    std::shared_ptr<MetricKindMap> metric_kind) {
  return std::unique_ptr<CheckAggregator>(
      new CheckAggregatorImpl(service_name, options, metric_kind));
}

}  // namespace service_control_client
}  // namespace google
