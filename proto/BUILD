licenses(["notice"])

load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "quotacontrol",
    srcs = [
        "google/api/servicecontrol/v1/quota_controller.proto",
    ],
    include = ".",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
        "//external:servicecontrol",
    ],
)
