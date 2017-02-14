licenses(["notice"])

cc_library(
    name = "http_transport",
    srcs = [
        "transport/http_transport.cc",
    ],
    hdrs = [
        "transport/http_transport.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@//:service_control_client_lib",
        "@//proto:servicecontrol",
    ],
)

cc_binary(
    name = "http_sample",
    srcs = [
        "transport/http_sample.cc",
    ],
    visibility = ["//visibility:public"],
    linkopts = [
        "-lcurl",
    ],
    deps = [
        ":http_transport",
        "@//:service_control_client_lib",
    ],
)
