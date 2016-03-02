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
      metric_kinds_(metric_kinds),
      flush_callback_(NULL) {
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
  MutexLock lock(callback_mutex_);
  flush_callback_ = callback;
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
    MutexLock lock(callback_mutex_);
    if (flush_callback_) {
      flush_callback_(request);
    }
    return Status::OK;
  }

  MutexLock lock(cache_mutex_);

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

  MutexLock lock(callback_mutex_);
  if (flush_callback_) {
    flush_callback_(request);
  }
}

// When the next Flush() should be called.
// Return in ms from now, or -1 for never
int ReportAggregatorImpl::GetNextFlushInterval() {
  return options_.flush_interval_ms;
}

// Flush aggregated requests whom are longer than flush_interval.
// Called at time specified by GetNextFlushInterval().
Status ReportAggregatorImpl::Flush() {
  MutexLock lock(cache_mutex_);
  if (cache_) {
    cache_->RemoveExpiredEntries();
  }
  return Status::OK;
}

// Flush out aggregated report requests, clear all cache items.
// Usually called at destructor.
Status ReportAggregatorImpl::FlushAll() {
  MutexLock lock(cache_mutex_);
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
