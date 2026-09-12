cmake_minimum_required(VERSION 3.16)
project(MGD_Emulator VERSION 0.1.0 LANGUAGES CXX)

# This is included from the parent CMakeLists.txt

# Vulkan detection
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    message(WARNING "Vulkan SDK not found - building GPU stub only")
    set(Vulkan_INCLUDE_DIRS "")
    set(Vulkan_LIBRARIES "")
else()
    message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
endif()

# Include directories
target_include_directories(mgd_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# ========== EMULATOR SOURCES ==========
set(EMULATOR_SOURCES
    # CPU
    cpu/Cpu.h

    # RAM
    ram/Mmu.h
    ram/GuestRam.h

    # HOS Services
    hos/Kernel.h
    hos/Session.h
    hos/PortRegistry.h
    hos/ServiceManager.h
    hos/AppletService.h
    hos/NvService.h
    hos/ViService.h
    hos/AudService.h
    hos/ApmService.h
    hos/AccService.h
    hos/FsService.h
    hos/HidService.h
    hos/LblService.h
    hos/PmService.h
    hos/PsmService.h
    hos/SetService.h
    hos/FatalService.h
    hos/TimeService.h
    hos/Hipc.h
    hos/Thread.h
    hos/Mutex.h
    hos/Event.h

    # Loader
    loader/Keys.h
    loader/NcaProbe.h
    loader/NcaSections.h
    loader/NsoLoader.h
    loader/NroLoader.h
    loader/Pfs0.h
    loader/RomFs.h
    loader/Lz4.h
    loader/Aes.h
    loader/AesNeon.h
    loader/Sha256.h
    loader/Npdm.h
    loader/Reloc.h

    # GPU
    gpu/Gpu.h
    gpu/Gpu.cpp
    gpu/VulkanBackend.h
    gpu/VulkanBackend.cpp

    # Core GPU (shared with main engine)
    core/gpu/Fsr10.h
    core/gpu/Fsr10.cpp
    core/gpu/FramebufferOptimizer.h
    core/gpu/FramebufferOptimizer.cpp
    core/gpu/PainterCompute.h
    core/gpu/PainterCompute.cpp
    core/gpu/AssetPipeline.h
    core/gpu/AssetPipeline.cpp
    core/gpu/Fsr2Compute.h
    core/gpu/Fsr2Compute.cpp
    core/gpu/Fsr2EasuCompute.h
    core/gpu/Fsr2EasuCompute.cpp
    core/gpu/Fsr2TaaCompute.cpp
    core/gpu/Fsr2RcasCompute.cpp
    core/gpu/FrustumCullingCompute.h
    core/gpu/FrustumCullingCompute.cpp
    core/gpu/FramebufferManager.h
    core/gpu/TileCache.cpp
    core/gpu/TileGrid.cpp
    core/gpu/DirtyRegionTracker.cpp
    core/gpu/MFOManager.cpp

    # Odyssey
    odyssey/OdysseyWorld.h
    odyssey/OdysseyHandoff.h

    # Handoff
    handoff/CaptureStub.h

    # Runtime
    runtime/Emulator.h
    runtime/MarioOdysseyRunner.h
    runtime/MarioOdysseyRunner.cpp
)

# The emulator sources are mostly header-only.
# Gpu.cpp and VulkanBackend.cpp are the only .cpp files (besides runner).
# They are compiled as part of the test executables that include them.

# Add Vulkan definitions for GPU code
if(Vulkan_FOUND)
    target_compile_definitions(mgd_core INTERFACE MGD_HAS_VULKAN)
    target_include_directories(mgd_core INTERFACE ${Vulkan_INCLUDE_DIRS})
endif()

# Add emulator test sources to mgd_tests
# (done in parent CMakeLists.txt via tests/test_emulator_*.cpp)

# ========== ANDROID BUILD ==========
# Android build is handled separately via android/CMakeLists.txt
# and gradle. This CMakeLists.txt is for desktop testing only.