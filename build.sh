#!/bin/bash
set -e

CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -Wall -Wextra"
BUILD_DIR=build

mkdir -p "$BUILD_DIR"

CORE_SOURCES=$(find core -name '*.cpp' | sort)
TEST_SOURCES=$(find tests -name '*.cpp' | sort)

$CXX $CXXFLAGS -I. -DMGD_TESTING $CORE_SOURCES $TEST_SOURCES -o "$BUILD_DIR/mgd_tests"

echo "Build OK -> $BUILD_DIR/mgd_tests"