licenses(["notice"])

cc_library(
    name = "service_control_client_lib",
    srcs = [
         "src/check_aggregator_impl.cc",
         "src/money_utils.cc",
         "src/operation_aggregator.cc",
         "src/report_aggregator_impl.cc",
         "src/service_control_client_impl.cc",
         "src/signature.cc",
         "src/check_aggregator_impl.h",
         "src/aggregator_interface.h",
         "src/money_utils.h",
         "src/operation_aggregator.h",
         "src/report_aggregator_impl.h",
         "src/service_control_client_impl.h",
         "src/signature.h",
         "src/cache_removed_items_handler.h",
         "utils/google_macros.h",
         "utils/status_test_util.h",
         "utils/stl_util.h",
         "utils/thread.h",
         "utils/distribution_helper.cc",
         "utils/md5.h",
         "utils/md5.cc",
    ],
    hdrs = [
          "include/aggregation_options.h",
          "include/periodic_timer.h",
          "include/service_control_client.h",
          "include/transport.h",
          "utils/distribution_helper.h",

          "utils/simple_lru_cache_inl.h",
          "utils/simple_lru_cache.h",
    ],
    visibility = ["//visibility:public"],

    # A hack to use this BUILD as part of other projects.
    # The other projects will add this module as third_party/service-control-client-cxx
    copts = ["-Ithird_party/service-control-client-cxx"],
    deps = [
          "//external:boringssl_crypto",
          "//third_party/config:servicecontrol",
    ],
)


cc_library(
    name = "distribution_helper_lib",
    srcs = [ "utils/distribution_helper.cc" ],
    hdrs = [ "utils/distribution_helper.h"  ],
    visibility = ["//visibility:public"],
    deps = [
         "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "simple_lru_cache",
    srcs = [ "utils/google_macros.h",
             "utils/status_test_util.h",
             "utils/stl_util.h",
    ],
    hdrs = [
         "utils/simple_lru_cache_inl.h",
         "utils/simple_lru_cache.h",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "distribution_helper_test",
    size = "small",
    srcs = [ "utils/distribution_helper_test.cc" ],
    deps = [
        ":distribution_helper_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "md5_test",
    size = "small",
    srcs = [ "utils/md5_test.cc" ],
    deps = [
         ":service_control_client_lib",
         "//external:googletest_main",
    ],
)

cc_test(
    name = "simple_lru_cache_test",
    srcs = [ "utils/simple_lru_cache_test.cc" ],
    size = "small",
    linkopts = [
         "-lm",
         "-lpthread",
    ],
    deps = [
        ":simple_lru_cache",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "check_aggregator_impl_test",
    size = "small",
    srcs = [ "src/check_aggregator_impl_test.cc" ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "src/money_utils_test.cc" ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "operation_aggregator_test",
    size = "small",
    srcs = [ "src/operation_aggregator_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "report_aggregator_impl_test",
    size = "small",
    srcs = [ "src/report_aggregator_impl_test.cc" ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "service_control_client_impl_test",
    size = "small",
    srcs = [ "src/service_control_client_impl_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "signature_test",
    size = "small",
    srcs = [ "src/signature_test.cc" ],
    deps = [
        ":service_control_client_lib",
        "//external:googletest_main",
    ],
)



