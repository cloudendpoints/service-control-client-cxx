
cc_library(
    name = "common_headers",
    hdrs = [
      "google_macros.h",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "service_control_proto_test",
    size = "small",
    srcs = [
        "service_control_proto_test.cc",
    ],
    deps = [
        "//external:googletest_main",
        "//third_party/config:servicecontrol",
    ],
)