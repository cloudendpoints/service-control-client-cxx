
cc_library(
    name = "client_lib",
    srcs = [
         "check_aggregator_impl.cc",
         "money_utils.cc",
         "operation_aggregator.cc",
         "report_aggregator_impl.cc",
         "signature.cc",
    ],
    hdrs = [
         "check_aggregator_impl.h",
         "aggregator_interface.h",
         "money_utils.h",
         "operation_aggregator.h",
         "report_aggregator_impl.h",
         "signature.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//utils:utils_lib",
    ],
)

cc_test(
    name = "check_aggregator_impl_test",
    size = "small",
    srcs = [ "check_aggregator_impl_test.cc" ],
    deps = [
        ":client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "report_aggregator_impl_test",
    size = "small",
    srcs = [ "report_aggregator_impl_test.cc" ],
    deps = [
        ":client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "operation_aggregator_test",
    size = "small",
    srcs = [ "operation_aggregator_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "money_utils_test.cc" ],
    deps = [
        ":client_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "signature_test",
    size = "small",
    srcs = [ "signature_test.cc" ],
    deps = [
        ":client_lib",
        "//external:googletest_main",
    ],
)

