cmake_minimum_required(VERSION 3.18)
project(mgd_odyssey LANGUAGES CXX)

# Android NDK settings
set(CMAKE_ANDROID_NDK ${CMAKE_ANDROID_NDK} CACHE PATH "Android NDK path")
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 24) # API level
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../emulador-mgd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core
)

# Source files
set(MGD_SOURCES
    # Emulator core
    ../../../../emulador-mgd/cpu/Cpu.h
    ../../../../emulador-mgd/ram/Mmu.h
    ../../../../emulador-mgd/ram/GuestRam.h
    ../../../../emulador-mgd/hos/Kernel.h
    ../../../../emulador-mgd/hos/*.h
    ../../../../emulador-mgd/loader/*.h
    ../../../../emulador-mgd/odyssey/*.h
    ../../../../emulador-mgd/runtime/*.h
    ../../../../emulador-mgd/gpu/*.h
    ../../../../emulador-mgd/re/*.h
    
    # Core GPU
    ../../../../core/gpu/*.h
    ../../../../core/gpu/*.cpp
    
    # Core bridge
    ../../../../core/bridge/*.h
    ../../../../core/bridge/*.cpp
    
    # Core query
    ../../../../core/query/*.h
    ../../../../core/query/*.cpp
    
    # Core scanner
    ../../../../core/scanner/*.h
    ../../../../core/scanner/*.cpp
)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Create shared library
add_library(mgd_odyssey SHARED
    jni_bridge.cpp
)

target_link_libraries(mgd_odyssey
    ${VULKAN_LIBRARIES}
    android
    log
    EGL
)

target_include_directories(mgd_odyssey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../emulador-mgd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core
    ${VULKAN_INCLUDE_DIRS}
)

target_compile_definitions(mgd_odyssey PRIVATE
    MGD_ANDROID
    MGD_HAS_VULKAN
)

# Link core library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../core/gpu core_gpu)
target_link_libraries(mgd_odyssey mgd_core)