licenses(["notice"])
load("/google/protobuf/protobuf", "cc_proto_library")

py_binary(
    name = "boringssl_error_gen",
    srcs = [
        "boringssl_error_gen.py",
    ],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "service_config",
    srcs = [
        "googleapis/google/api/annotations.proto",
        "googleapis/google/api/auth.proto",
        "googleapis/google/api/backend.proto",
        "googleapis/google/api/billing.proto",
        "googleapis/google/api/consumer.proto",
        "googleapis/google/api/context.proto",
        "googleapis/google/api/control.proto",
        "googleapis/google/api/documentation.proto",
        "googleapis/google/api/http.proto",
        "googleapis/google/api/label.proto",
        "googleapis/google/api/log.proto",
        "googleapis/google/api/logging.proto",
        "googleapis/google/api/metric.proto",
        "googleapis/google/api/monitored_resource.proto",
        "googleapis/google/api/monitoring.proto",
        "googleapis/google/api/service.proto",
        "googleapis/google/api/system_parameter.proto",
        "googleapis/google/api/usage.proto",
    ],
    include = "googleapis",
    visibility = ["//visibility:public"],
    deps = [
        "//google/protobuf:cc_wkt_protos",
    ],
)