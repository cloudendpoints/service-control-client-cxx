
cc_library(
    name = "common_headers",
    hdrs = [
      "google_macros.h",
      "status_test_util.h",
      "stl_util.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "aggregator_lib",
    srcs = [
         "check_aggregator_impl.cc",
         "distribution_helper.cc",
         "md5.cc",
         "money_utils.cc",
         "operation_aggregator.cc",
         "report_aggregator_impl.cc",
         "signature.cc",
    ],
    hdrs = [
         "check_aggregator_impl.h",
         "aggregator_interface.h",
         "distribution_helper.h",
         "md5.h",
         "money_utils.h",
         "operation_aggregator.h",
         "report_aggregator_impl.h",
         "signature.h",
         "thread.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":common_headers",
        "//cache:simple_lru_cache",
        "//external:ssl_crypto",
        "//third_party/config:servicecontrol",
    ],
)

cc_test(
    name = "check_aggregator_impl_test",
    size = "small",
    srcs = [ "check_aggregator_impl_test.cc" ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "report_aggregator_impl_test",
    size = "small",
    srcs = [ "report_aggregator_impl_test.cc" ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "operation_aggregator_test",
    size = "small",
    srcs = [ "operation_aggregator_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "money_utils_test.cc" ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "distribution_helper_test",
    size = "small",
    srcs = [ "distribution_helper_test.cc" ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "md5_test",
    size = "small",
    srcs = [ "md5_test.cc" ],
    deps = [
        ":aggregator_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "signature_test",
    size = "small",
    srcs = [ "signature_test.cc" ],
    deps = [
        ":aggregator_lib",
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
