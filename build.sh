#!/bin/bash
set -e

CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -Wall -Wextra -DMGD_TESTING"
BUILD_DIR=build

mkdir -p "$BUILD_DIR"

# Core sources (emulador-mgd + core/gpu)
CORE_SOURCES="
emulador-mgd/core/gpu/Fsr10.cpp
emulador-mgd/gpu/Gpu.cpp
emulador-mgd/gpu/VulkanBackend.cpp
emulador-mgd/runtime/MarioOdysseyRunner.cpp
"

# Test sources
TEST_SOURCES="
tests/test_applet_service.cpp
tests/test_emulator_odyssey.cpp
tests/test_integration_full.cpp
tests/test_ipc_buffers.cpp
tests/test_nca_decrypt.cpp
tests/test_save_state.cpp
"

echo "Building mgd_core..."
# Compile each source file individually to object files
CORE_OBJECTS=""
for src in $CORE_SOURCES; do
    obj_name="$BUILD_DIR/$(basename "$src" .cpp).o"
    $CXX $CXXFLAGS -I. -DMGD_TESTING "$src" -c -o "$obj_name" || exit 1
    CORE_OBJECTS="$obj_name $CORE_OBJECTS"
done

echo "Building mgd_tests..."
# Link all core object files with test sources and Catch2
$CXX $CXXFLAGS -I. -DMGD_TESTING $CORE_OBJECTS $TEST_SOURCES -o "$BUILD_DIR/mgd_tests" -lCatch2Main -lCatch2 || exit 1

echo "Build OK -> $BUILD_DIR/mgd_tests"