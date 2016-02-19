# A Bazel (http://bazel.io) workspace for the Google Service Control client

new_git_repository(
    name = "googletest_git",
    build_file = "BUILD.googletest",
    commit = "13206d6f53aaff844f2d3595a01ac83a29e383db",
    remote = "https://github.com/google/googletest.git",
)

bind(
    name = "googletest",
    actual = "@googletest_git//:googletest",
)

bind(
    name = "googletest_main",
    actual = "@googletest_git//:googletest_main",
)

# Reimplementation of error table generator Boring SSL uses in build.
# Boring SSL implementation is in go which doesn't yet have complete Bazel
# support and the temporary support used in nginx workspace
# https://nginx.googlesource.com/workspace does not work well with
# Bazel sandboxing. Therefore, we temporarily reimplement the error
# table generator.
bind(
    name = "boringssl_error_gen",
    actual = "//tools:boringssl_error_gen",
)

new_git_repository(
    name = "boringssl_git",
    build_file = "BUILD.boringssl",
    commit = "c4f25ce0c6e3822eb639b2a5649a51ea42b46490",
    remote = "https://boringssl.googlesource.com/boringssl",
)

bind(
    name = "ssl_crypto",
    actual = "@boringssl_git//:crypto",
)
