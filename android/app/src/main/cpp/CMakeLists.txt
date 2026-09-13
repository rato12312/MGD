cmake_minimum_required(VERSION 3.18)
project(mgd_odyssey LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Android NDK settings
set(CMAKE_ANDROID_NDK ${CMAKE_ANDROID_NDK} CACHE PATH "Android NDK path")
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 24)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_STL_TYPE c++_shared)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../emulador-mgd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core
)

# Find Vulkan
find_package(Vulkan REQUIRED)

# ========== EMULATOR SOURCES ==========
set(EMULATOR_SOURCES
    # CPU
    ../../../../emulador-mgd/cpu/Cpu.h

    # RAM
    ../../../../emulador-mgd/ram/Mmu.h
    ../../../../emulador-mgd/ram/GuestRam.h

    # HOS Services
    ../../../../emulador-mgd/hos/Kernel.h
    ../../../../emulador-mgd/hos/Session.h
    ../../../../emulador-mgd/hos/PortRegistry.h
    ../../../../emulador-mgd/hos/ServiceManager.h
    ../../../../emulador-mgd/hos/AppletService.h
    ../../../../emulador-mgd/hos/NvService.h
    ../../../../emulador-mgd/hos/ViService.h
    ../../../../emulador-mgd/hos/AudService.h
    ../../../../emulador-mgd/hos/ApmService.h
    ../../../../emulador-mgd/hos/AccService.h
    ../../../../emulador-mgd/hos/FsService.h
    ../../../../emulador-mgd/hos/HidService.h
    ../../../../emulador-mgd/hos/LblService.h
    ../../../../emulador-mgd/hos/PmService.h
    ../../../../emulador-mgd/hos/PsmService.h
    ../../../../emulador-mgd/hos/SetService.h
    ../../../../emulador-mgd/hos/FatalService.h
    ../../../../emulador-mgd/hos/TimeService.h
    ../../../../emulador-mgd/hos/Session.h
    ../../../../emulador-mgd/hos/PortRegistry.h
    ../../../../emulador-mgd/hos/ServiceManager.h
    ../../../../emulador-mgd/hos/Thread.h
    ../../../../emulador-mgd/hos/Mutex.h
    ../../../../emulador-mgd/hos/Event.h

    # Loader
    ../../../../emulador-mgd/loader/Keys.h
    ../../../../emulador-mgd/loader/NcaProbe.h
    ../../../../emulador-mgd/loader/NcaSections.h
    ../../../../emulador-mgd/loader/NsoLoader.h
    ../../../../emulador-mgd/loader/NroLoader.h
    ../../../../emulador-mgd/loader/Pfs0.h
    ../../../../emulador-mgd/loader/RomFs.h
    ../../../../emulador-mgd/loader/Lz4.h
    ../../../../emulador-mgd/loader/Aes.h
    ../../../../emulador-mgd/loader/AesNeon.h
    ../../../../emulador-mgd/loader/Sha256.h
    ../../../../emulador-mgd/loader/Npdm.h
    ../../../../emulador-mgd/loader/Reloc.h

    # GPU
    ../../../../emulador-mgd/gpu/Gpu.cpp
    ../../../../emulador-mgd/gpu/VulkanBackend.cpp

    # Core GPU (shared with main engine)
    ../../../../core/gpu/Fsr10.cpp
    ../../../../core/gpu/FramebufferOptimizer.cpp
    ../../../../core/gpu/PainterCompute.cpp
    ../../../../core/gpu/AssetPipeline.cpp
    ../../../../core/gpu/Fsr2EasuCompute.cpp
    ../../../../core/gpu/Fsr2TaaCompute.cpp
    ../../../../core/gpu/Fsr2RcasCompute.cpp
    ../../../../core/gpu/FrustumCullingCompute.cpp
    ../../../../core/gpu/TileCache.cpp
    ../../../../core/gpu/TileGrid.cpp
    ../../../../core/gpu/DirtyRegionTracker.cpp
    ../../../../core/gpu/MFOManager.cpp

    # Odyssey
    ../../../../emulador-mgd/odyssey/OdysseyWorld.h
    ../../../../emulador-mgd/odyssey/OdysseyHandoff.h

    # Handoff
    ../../../../emulador-mgd/handoff/CaptureStub.h

    # Runtime
    ../../../../emulador-mgd/runtime/Emulator.cpp
    ../../../../emulador-mgd/runtime/MarioOdysseyRunner.cpp
)

# Core GPU (shared with main engine)
set(CORE_GPU_SOURCES
    ../../../../core/gpu/Fsr10.cpp
    ../../../../core/gpu/FramebufferOptimizer.cpp
    ../../../../core/gpu/PainterCompute.cpp
    ../../../../core/gpu/AssetPipeline.cpp
    ../../../../core/gpu/Fsr2EasuCompute.cpp
    ../../../../core/gpu/Fsr2TaaCompute.cpp
    ../../../../core/gpu/Fsr2RcasCompute.cpp
    ../../../../core/gpu/FrustumCullingCompute.cpp
    ../../../../core/gpu/TileCache.cpp
    ../../../../core/gpu/TileGrid.cpp
    ../../../../core/gpu/DirtyRegionTracker.cpp
    ../../../../core/gpu/MFOManager.cpp
    ../../../../core/gpu/SeedPredictor.cpp
)

add_library(mgd_odyssey SHARED
    ${EMULATOR_SOURCES}
    ${CORE_GPU_SOURCES}
    ../../../../emulador-mgd/runtime/MarioOdysseyRunner.cpp
    ../../../../emulador-mgd/gpu/Gpu.cpp
    ../../../../emulador-mgd/gpu/VulkanBackend.cpp
)

target_include_directories(mgd_odyssey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../emulador-mgd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(mgd_odyssey
    ${VULKAN_LIBRARIES}
    android
    log
    EGL
)

if(Vulkan_FOUND)
    target_compile_definitions(mgd_odyssey PRIVATE MGD_HAS_VULKAN)
    target_include_directories(mgd_odyssey PRIVATE ${VULKAN_INCLUDE_DIRS})
endif()

target_compile_definitions(mgd_odyssey PRIVATE
    MGD_ANDROID
    MGD_HAS_VULKAN
)

target_compile_options(mgd_odyssey PRIVATE
    -std=c++17
    -frtti
    -fexceptions
    -DMGD_ANDROID
    -DMGD_HAS_VULKAN
)