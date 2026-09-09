cmake_minimum_required(VERSION 3.18)

# MGD Odyssey Android - Native Library
project(MGDOdyssey LANGUAGES CXX C)

# Android config
set(CMAKE_ANDROID_API 30)  # Android 11+
set(CMAKE_ANDROID_STL c++17_shared)
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Policy para evitar warnings
cmake_policy(SET CMP0074 NEW)

# Build types
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/../core
    ${CMAKE_SOURCE_DIR}/../emulador-mgd
    ${CMAKE_SOURCE_DIR}/../core/gpu
    ${VULKAN_INCLUDE_DIRS}
)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Source files
set(MGD_CORE_SOURCES
    ../core/common/Vec3.cpp
    ../core/common/Vec4.cpp
    ../core/common/Mat4.cpp
    ../core/common/Frustum.cpp
    ../core/common/AABB.cpp
    ../core/common/Plane.cpp
    ../core/common/Ray.cpp
    ../core/camera/Camera.cpp
    ../core/camera/CameraController.cpp
    ../core/collision/CollisionSystem.cpp
    ../core/collision/spatial/GridIndex.cpp
    ../core/collision/queries/Raycast.cpp
    ../core/mental_map/MentalMap.cpp
    ../core/mental_map/Entity.cpp
    ../core/mental_map/Region.cpp
    ../core/mental_map/ChunkManager.cpp
    ../core/mental_map/builder/MentalMapBuilder.cpp
    ../core/mental_map/events/EventManager.cpp
    ../core/visibility/BasicVisibility.cpp
    ../core/query/RegionPolygonCache.cpp
    ../core/query/PolygonConsultant.cpp
    ../core/query/dna/DnaPipeline.cpp
    ../core/query/dna/ChangeDetector.cpp
    ../core/query/dna/PixelCache.cpp
    ../core/query/dna/PixelMap.cpp
    ../core/query/dna/ColorCode.cpp
    ../core/query/dna/XyzIndex.cpp
    ../core/query/dna/Dna.cpp
    ../core/bridge/EmulatorHandoff.cpp
    ../core/bridge/MentalMapRuntime.cpp
    ../core/scanner/FileDiscovery.cpp
    ../core/scanner/HashService.cpp
    ../core/scanner/AssetRegistry.cpp
    ../core/scanner/DependencyResolver.cpp
    ../core/scanner/ScanProgress.cpp
    ../core/scanner/Scanner.cpp
    ../core/scanner/normalize/Infector.cpp
    ../core/scanner/analyzers/TextureAnalyzer.cpp
    ../core/scanner/analyzers/MeshAnalyzer.cpp
    ../core/scanner/analyzers/MetadataAnalyzer.cpp
    ../core/scanner/analyzers/BSAAnalyzer.cpp
    ../core/scanner/analyzers/ESPAnalyzer.cpp
    ../core/scanner/adapters/SkyrimPCAdapter.cpp
    ../core/scanner/adapters/SkyrimXbox360Adapter.cpp
    ../core/painter/framebuffer/Framebuffer.cpp
    ../core/painter/rasterizer/Rasterizer.cpp
    ../core/painter/shader/FrameGovernor.cpp
    ../core/renderer/renderer.cpp
    ../core/renderer/SDL2Presenter.cpp
    ../core/cache/CacheEntry.cpp
)

set(MGD_EMULADOR_SOURCES
    ../emulador-mgd/cpu/Cpu.cpp
    ../emulador-mgd/loader/Aes.cpp
    ../emulador-mgd/loader/Lz4.cpp
    ../emulador-mgd/loader/Sha256.cpp
    ../emulador-mgd/loader/Pfs0.cpp
    ../emulador-mgd/loader/RomFs.cpp
    ../emulador-mgd/loader/NroLoader.cpp
    ../emulador-mgd/loader/NsoLoader.cpp
    ../emulador-mgd/loader/NcaProbe.cpp
    ../emulador-mgd/loader/NcaSections.cpp
    ../emulador-mgd/loader/Reloc.cpp
    ../emulador-mgd/loader/Keys.cpp
    ../emulador-mgd/loader/Npdm.cpp
    ../emulador-mgd/hos/Session.cpp
    ../emulador-mgd/hos/PortRegistry.cpp
    ../emulador-mgd/hos/ServiceManager.cpp
    ../emulador-mgd/hos/Thread.cpp
    ../emulador-mgd/hos/Mutex.cpp
    ../emulador-mgd/hos/Event.cpp
    ../emulador-mgd/hos/Kernel.cpp
    ../emulador-mgd/hos/NvService.cpp
    ../emulador-mgd/hos/ViService.cpp
    ../emulador-mgd/hos/AudService.cpp
    ../emulador-mgd/hos/FsService.cpp
    ../emulador-mgd/hos/HidService.cpp
    ../emulador-mgd/hos/TimeService.cpp
    ../emulador-mgd/hos/ApmService.cpp
    ../emulador-mgd/hos/PsmService.cpp
    ../emulador-mgd/hos/LblService.cpp
    ../emulador-mgd/hos/SetService.cpp
    ../emulador-mgd/hos/FatalService.cpp
    ../emulador-mgd/hos/PmService.cpp
    ../emulador-mgd/hos/AccService.cpp
    ../emulador-mgd/hos/AppletService.cpp
    ../emulador-mgd/ram/Mmu.cpp
    ../emulador-mgd/ram/GuestRam.cpp
    ../emulador-mgd/odyssey/OdysseyWorld.cpp
    ../emulador-mgd/odyssey/OdysseyHandoff.cpp
    ../emulador-mgd/runtime/Emulator.cpp
    ../emulador-mgd/config/MgdSwitches.cpp
    ../emulador-mgd/handoff/CaptureStub.cpp
)

set(MGD_GPU_SOURCES
    ../core/gpu/FramebufferManager.cpp
    ../core/gpu/PainterCompute.cpp
    ../core/gpu/AssetPipeline.cpp
    ../emulador-mgd/gpu/VulkanBackend.cpp
    ../emulador-mgd/gpu/Gpu.cpp
)

# Shared library para Android (GameActivity / NativeActivity)
add_library(mgd_odyssey SHARED
    ${MGD_CORE_SOURCES}
    ${MGD_EMULADOR_SOURCES}
    ${MGD_GPU_SOURCES}
    android_main.cpp
)

# Link libraries
target_link_libraries(mgd_odyssey
    ${VULKAN_LIBRARIES}
    log
    android
    EGL
    GLESv3
)

# Compile definitions
target_compile_definitions(mgd_odyssey PRIVATE
    VK_USE_PLATFORM_ANDROID_KHR
    MGD_ANDROID
    MGD_HAS_VULKAN=1
    _GLIBCXX_USE_CXX11_ABI=1
)

# Android-specific
set_target_properties(mgd_odyssey PROPERTIES
    ANDROID_SO_UNDEF_ONLY FALSE
)

# Install
install(TARGETS mgd_odyssey
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)