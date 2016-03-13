# The Service Control Client library for c/c++ #

The Google service control server provides essential API services
such as billing, quota management and monitoring.

This repository contains the client library for c/c++ to talk to
the service control service.

[TOC]


## Getting Service Control Client library ##

To download the service control client source code, clone the repository:

    # Clone the repository
    git clone sso://gcp-apis/service-control-client-cxx

## Repository Structure ##

* [include](/include): The folder contains public headers.
* [utils](/utils): The folder contains utility code.
* [src](/src): The folder contains core source code.

## Setup, build and test ##

Recommended workflow to setup, build and test service control client code:

    # Sync your git repository with the origin.
    git checkout master
    git pull origin --rebase

    # Setup git for remote push
    script/setup

    # Updates submodules
    git submodule update --init --recursive

    # Use Bazel to build
    bazel build //utils:all
    bazel build //src:all

    # Use Bazel to test
    bazel test //utils:all
    bazel test //src:all



