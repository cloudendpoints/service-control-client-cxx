
cc_library(
    name = "common_headers",
    hdrs = [
      "google_macros.h",
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
