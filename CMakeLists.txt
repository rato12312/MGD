cmake_minimum_required(VERSION 3.16)
project(MGD_Engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Catch2 for integration tests (FetchContent)
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
)
FetchContent_MakeAvailable(Catch2)

# SDL2 detection
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
endif()

if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR SDL.h
        PATHS
        "E:/mgd_tools/SDL2-2.30.12/x86_64-w64-mingw32/include"
        "C:/SDL2/include"
        /usr/include
        /usr/local/include
    )
    find_library(SDL2_LIBRARY SDL2
        PATHS
        "E:/mgd_tools/SDL2-2.30.12/x86_64-w64-mingw32/lib"
        "C:/SDL2/lib"
        /usr/lib
        /usr/local/lib
    )
    if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    else()
        message(WARNING "SDL2 not found - building without SDL2 (tests only)")
        set(SDL2_INCLUDE_DIRS "")
        set(SDL2_LIBRARIES "")
    endif()
endif()

# Vulkan detection
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
    set(VULKAN_INCLUDE_DIRS ${Vulkan_INCLUDE_DIRS})
    set(VULKAN_LIBRARIES ${Vulkan_LIBRARIES})
else()
    message(WARNING "Vulkan SDK not found - GPU backend will use stub")
    set(VULKAN_INCLUDE_DIRS "")
    set(VULKAN_LIBRARIES "")
endif()

# Include root for <emulador-mgd/...> style includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ========== CORE LIBRARY (emulador-mgd only) ==========
set(MGD_CORE_SOURCES
    # GPU Core (shared with emulator)
    emulador-mgd/core/gpu/Fsr10.cpp
    emulador-mgd/core/gpu/Fsr10.h
    
    # Emulator GPU sources
    emulador-mgd/gpu/Gpu.cpp
    emulador-mgd/gpu/VulkanBackend.cpp
    emulador-mgd/runtime/MarioOdysseyRunner.cpp
)

# Loader (header-only)
set(LOADER_SOURCES
    emulador-mgd/loader/NcaDecrypt.h
)
target_sources(mgd_core PUBLIC ${LOADER_SOURCES})

add_library(mgd_core STATIC ${MGD_CORE_SOURCES})
target_include_directories(mgd_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(SDL2_FOUND)
    target_include_directories(mgd_core PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(mgd_core PUBLIC ${SDL2_LIBRARIES})
    target_compile_definitions(mgd_core PUBLIC MGD_HAS_SDL2)
endif()

if(Vulkan_FOUND)
    target_include_directories(mgd_core PUBLIC ${VULKAN_INCLUDE_DIRS})
    target_link_libraries(mgd_core PUBLIC ${VULKAN_LIBRARIES})
    target_compile_definitions(mgd_core PUBLIC MGD_HAS_VULKAN)
endif()

# ========== TESTS ==========
enable_testing()
add_executable(mgd_tests
    tests/test_applet_service.cpp
    tests/test_emulator_odyssey.cpp
    tests/test_integration_full.cpp
    tests/test_ipc_buffers.cpp
    tests/test_nca_decrypt.cpp
    tests/test_save_state.cpp
)
target_link_libraries(mgd_tests mgd_core Catch2::Catch2WithMain)
target_compile_definitions(mgd_tests PRIVATE MGD_TESTING)
if(Vulkan_FOUND)
    target_link_libraries(mgd_tests ${VULKAN_LIBRARIES})
endif()
add_test(NAME mgd_unit_tests COMMAND mgd_tests)
add_test(NAME mgd_integration_tests COMMAND mgd_tests)

# ========== INSTALL ==========
install(TARGETS mgd_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY emulador-mgd/
    DESTINATION include/mgd
    FILES_MATCHING PATTERN "*.h"
)