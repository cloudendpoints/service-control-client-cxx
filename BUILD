
cc_library(
    name = "common_headers",
    hdrs = [
      "google_macros.h",
      "status_test_util.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "distribution_helper_lib",
    srcs = [ "distribution_helper.cc" ],
    hdrs = [ "distribution_helper.h" ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "md5_lib",
    srcs = [ "md5.cc" ],
    hdrs = [ "md5.h" ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:ssl_crypto",
    ],
)

cc_library(
    name = "signature_lib",
    srcs = [ "signature.cc" ],
    hdrs = [ "signature.h" ],
    visibility = ["//visibility:public"],
    deps = [
        ":md5_lib",
        "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "money_utils_lib",
    srcs = [ "money_utils.cc" ],
    hdrs = [ "money_utils.h" ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "operation_aggregator_lib",
    srcs = [ "operation_aggregator.cc" ],
    hdrs = [ "operation_aggregator.h" ],
    visibility = ["//visibility:public"],
    deps = [
        ":common_headers",
        ":distribution_helper_lib",
        ":money_utils_lib",
        ":signature_lib",
    ],
)

cc_test(
    name = "operation_aggregator_test",
    size = "small",
    srcs = [ "operation_aggregator_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":operation_aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "money_utils_test.cc" ],
    deps = [
        ":common_headers",
        ":money_utils_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "distribution_helper_test",
    size = "small",
    srcs = [ "distribution_helper_test.cc" ],
    deps = [
        ":distribution_helper_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "md5_test",
    size = "small",
    srcs = [ "md5_test.cc" ],
    deps = [
        ":md5_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "signature_test",
    size = "small",
    srcs = [ "signature_test.cc" ],
    deps = [
        ":signature_lib",
        "//external:googletest_main",
    ],
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
