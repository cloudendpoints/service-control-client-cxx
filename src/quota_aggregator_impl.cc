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

#include <iostream>

#include "src/quota_aggregator_impl.h"
#include "src/signature.h"

#include "google/protobuf/stubs/logging.h"
#include "google/protobuf/text_format.h"

using std::string;
using ::google::api::MetricDescriptor;
using ::google::api::servicecontrol::v1::QuotaOperation;
using ::google::api::servicecontrol::v1::AllocateQuotaRequest;
using ::google::api::servicecontrol::v1::AllocateQuotaResponse;
using ::google::api::servicecontrol::v1::QuotaOperation_QuotaMode;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::google::service_control_client::SimpleCycleTimer;

namespace google {
namespace service_control_client {

void QuotaAggregatorImpl::CacheElem::Aggregate(
    const AllocateQuotaRequest& request) {
  if (operation_aggregator_ == NULL) {
    operation_aggregator_.reset(
        new QuotaOperationAggregator(request.allocate_operation()));
  } else {
    operation_aggregator_->MergeOperation(request.allocate_operation());
  }
}

AllocateQuotaRequest
QuotaAggregatorImpl::CacheElem::ReturnAllocateQuotaRequestAndClear(
    const string& service_name, const std::string& service_config_id) {
  AllocateQuotaRequest request;

  request.set_service_name(service_name);
  request.set_service_config_id(service_config_id);

  if (operation_aggregator_ != NULL) {
    *(request.mutable_allocate_operation()) =
        operation_aggregator_->ToOperationProto();
    operation_aggregator_ = NULL;
  } else {
    // If requests are not aggregated, use the stored initial request
    // to allocate minimum token
    request = quota_request_;
  }

  return request;
}

QuotaAggregatorImpl::QuotaAggregatorImpl(const std::string& service_name,
                                         const std::string& service_config_id,
                                         const QuotaAggregationOptions& options)
    : service_name_(service_name),
      service_config_id_(service_config_id),
      options_(options),
      in_flush_all_(false) {
  if (options.num_entries > 0) {
    cache_.reset(new QuotaCache(
        options.num_entries, std::bind(&QuotaAggregatorImpl::OnCacheEntryDelete,
                                       this, std::placeholders::_1)));
    cache_->SetAgeBasedEviction(options.refresh_interval_ms / 1000.0);
  }

  refresh_interval_in_cycle_ =
      options_.refresh_interval_ms * SimpleCycleTimer::Frequency() / 1000;

  expiration_interval_in_cycle_ =
      options_.expiration_interval_ms * SimpleCycleTimer::Frequency() / 1000;
}

QuotaAggregatorImpl::~QuotaAggregatorImpl() {
  SetFlushCallback(NULL);
  FlushAll();
}

// Sets the flush callback function.
// The callback function must be light and fast.  If it needs to make
// a remote call, it must be non-blocking call.
// It should NOT call into this object again from this callback.
// It will cause dead-lock.
void QuotaAggregatorImpl::SetFlushCallback(FlushCallback callback) {
  InternalSetFlushCallback(callback);
}

// If the quota could not be handled by the cache, returns NOT_FOUND,
// caller has to send the request to service control.
// Otherwise, returns OK and cached response.
::google::protobuf::util::Status QuotaAggregatorImpl::Quota(
    const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request,
    ::google::api::servicecontrol::v1::AllocateQuotaResponse* response) {
  if (request.service_name() != service_name_) {
    return Status(Code::INVALID_ARGUMENT,
                  (string("Invalid service name: ") + request.service_name() +
                   string(" Expecting: ") + service_name_));
  }

  if (!request.has_allocate_operation()) {
    return Status(Code::INVALID_ARGUMENT,
                  "allocate operation field is required.");
  }

  if (!cache_) {
    // By returning NO_FOUND, caller will send request to server.
    return Status(Code::NOT_FOUND, "");
  }

  AllocateQuotaCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer::Swapper swapper(
      this, &stack_buffer);

  string request_signature = GenerateAllocateQuotaRequestSignature(request);

  QuotaCache::ScopedLookup lookup(cache_.get(), request_signature);
  if (!lookup.Found()) {
    // To avoid sending concurrent allocateQuota from concurrent requests.
    // insert a temporary positive response to the cache. Requests from other
    // requests will be aggregated to this temporary element until the
    // response for the actual request arrives.
    ::google::api::servicecontrol::v1::AllocateQuotaResponse temp_response;
    CacheElem* cache_elem = new CacheElem(request, temp_response,
                                          SimpleCycleTimer::Now());
    cache_elem->set_signature(request_signature);
    cache_elem->set_in_flight(true);
    cache_->Insert(request_signature, cache_elem, 1);

    // Triggers refresh
    AddRemovedItem(request);

    // return positive response
    *response = cache_elem->quota_response();
    return ::google::protobuf::util::Status::OK;
  }

  if (lookup.value()->in_flight() == false &&
      ShouldRefresh(*lookup.value()) == true) {
    // Update in_flight to avoid duplicated request
    lookup.value()->set_in_flight(true);
    lookup.value()->set_last_refresh_time(SimpleCycleTimer::Now());

    AllocateQuotaRequest refresh_request =
        lookup.value()->ReturnAllocateQuotaRequestAndClear(service_name_,
                                                           service_config_id_);
    if(!lookup.value()->is_positive_response()) {
      // If the cached response is negative, then use NORMAL QuotaMode
      // instead of BEST_EFFORT
      refresh_request.mutable_allocate_operation()->
          set_quota_mode(
              QuotaOperation_QuotaMode::QuotaOperation_QuotaMode_NORMAL);
    }

    // Triggers refresh
    AddRemovedItem(refresh_request);
  }

  // Aggregate tokens if the cached response is positive
  if (lookup.value()->is_positive_response()) {
    lookup.value()->Aggregate(request);
  }

  *response = lookup.value()->quota_response();
  return ::google::protobuf::util::Status::OK;
}

// Caches a response from a remote Service Controller AllocateQuota call.
::google::protobuf::util::Status QuotaAggregatorImpl::CacheResponse(
    const ::google::api::servicecontrol::v1::AllocateQuotaRequest& request,
    const ::google::api::servicecontrol::v1::AllocateQuotaResponse& response) {
  if (!cache_) {
    return ::google::protobuf::util::Status::OK;
  }

  string request_signature = GenerateAllocateQuotaRequestSignature(request);

  AllocateQuotaCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer::Swapper swapper(
      this, &stack_buffer);

  QuotaCache::ScopedLookup lookup(cache_.get(), request_signature);
  if (lookup.Found()) {
    lookup.value()->set_in_flight(false);
    lookup.value()->set_quota_response(response);
  }

  return ::google::protobuf::util::Status::OK;
}

// When the next Flush() should be called.
// Returns in ms from now, or -1 for never
int QuotaAggregatorImpl::GetNextFlushInterval() {
  if (!cache_) return -1;
  return options_.refresh_interval_ms;
}

// Check the cached element should be refreshed
bool QuotaAggregatorImpl::ShouldRefresh(const CacheElem& elem) const {
  int64_t age = SimpleCycleTimer::Now() - elem.last_refresh_time();
  return age >= refresh_interval_in_cycle_;
}

// Check the cached element should be dropped from the cache
bool QuotaAggregatorImpl::ShouldDrop(const CacheElem& elem) const {
  int64_t age = SimpleCycleTimer::Now() - elem.last_refresh_time();
  return age >= expiration_interval_in_cycle_;
}

// Invalidates expired allocate quota responses.
// Called at time specified by GetNextFlushInterval().
::google::protobuf::util::Status QuotaAggregatorImpl::Flush() {
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer::Swapper swapper(
      this, &stack_buffer);

  if (cache_) {
    cache_->RemoveExpiredEntries();
  }

  return Status::OK;
}

// Flushes out all cached check responses; clears all cache items.
// Usually called at destructor.
::google::protobuf::util::Status QuotaAggregatorImpl::FlushAll() {
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer stack_buffer(this);
  MutexLock lock(cache_mutex_);
  AllocateQuotaCacheRemovedItemsHandler::StackBuffer::Swapper swapper(
      this, &stack_buffer);

  in_flush_all_ = true;

  GOOGLE_LOG(INFO) << "Remove all entries of check aggregator.";
  if (cache_) {
    cache_->RemoveAll();
  }

  return Status::OK;
}

// OnCacheEntryDelete will be called behind the cache_mutex_
// no need to consider locking at this point
void QuotaAggregatorImpl::OnCacheEntryDelete(CacheElem* elem) {
  if (in_flush_all_ == false && ShouldDrop(*elem) == false) {
    // insert the element back to the cache
    cache_->Insert(elem->signature(), elem, 1);

    if (elem->in_flight() == false && elem->is_aggregated()) {
      elem->set_in_flight(true);
      elem->set_last_refresh_time(SimpleCycleTimer::Now());
      AddRemovedItem(elem->ReturnAllocateQuotaRequestAndClear(
        service_name_, service_config_id_));
    }
  } else {
    delete elem;
  }
}

std::unique_ptr<QuotaAggregator> CreateAllocateQuotaAggregator(
    const std::string& service_name, const std::string& service_config_id,
    const QuotaAggregationOptions& options) {
  return std::unique_ptr<QuotaAggregator>(
      new QuotaAggregatorImpl(service_name, service_config_id, options));
}

}  // namespace service_control_client
}  // namespace google
