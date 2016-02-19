# Reimplementation of error table generator Boring SSL uses in build.
# Boring SSL implementation is in go which doesn't yet have complete Bazel
# support and the temporary support used in nginx workspace
# https://nginx.googlesource.com/workspace does not work well with
# Bazel sandboxing. Therefore, we temporarily reimplement the error
# table generator.

py_binary(
    name = "boringssl_error_gen",
    srcs = [
        "boringssl_error_gen.py",
    ],
    visibility = ["//visibility:public"],
)
