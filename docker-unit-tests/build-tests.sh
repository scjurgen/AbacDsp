#!/bin/bash

set -ex

cd /build

# Build only tests without JUCE examples to avoid GUI dependencies
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY_TESTS=ON -B /build /root/project

ninja unit_tests

