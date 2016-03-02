
cc_library(
    name = "service_control_client_internal_lib",
    srcs = [
         "check_aggregator_impl.cc",
         "money_utils.cc",
         "operation_aggregator.cc",
         "report_aggregator_impl.cc",
         "service_control_client_impl.cc",
         "signature.cc",
    ],
    hdrs = [
         "check_aggregator_impl.h",
         "aggregator_interface.h",
         "money_utils.h",
         "operation_aggregator.h",
         "report_aggregator_impl.h",
         "service_control_client_impl.h",
         "signature.h",
    ],
    # TODO(qiwzhang): Change it to private.
    visibility = ["//visibility:public"],
    deps = [
        "//include:public_headers",
        "//utils:utils_lib",
    ],
)

cc_test(
    name = "check_aggregator_impl_test",
    size = "small",
    srcs = [ "check_aggregator_impl_test.cc" ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "money_utils_test.cc" ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "operation_aggregator_test",
    size = "small",
    srcs = [ "operation_aggregator_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "report_aggregator_impl_test",
    size = "small",
    srcs = [ "report_aggregator_impl_test.cc" ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "service_control_client_impl_test",
    size = "small",
    srcs = [ "service_control_client_impl_test.cc" ],
    linkopts = [ "-lm", ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "signature_test",
    size = "small",
    srcs = [ "signature_test.cc" ],
    deps = [
        ":service_control_client_internal_lib",
        "//external:googletest_main",
    ],
)

