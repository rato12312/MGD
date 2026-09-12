cmake_minimum_required(VERSION 3.16)
project(MGD_Engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags - relaxed for initial build
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

# Include root for <core/...> style includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ========== CORE LIBRARY ==========
set(MGD_CORE_SOURCES
    # Common
    core/common/Mat4.cpp
    core/common/Vec4.cpp

    # Mental Map
    core/mental_map/MentalMap.cpp
    core/mental_map/builder/MentalMapBuilder.cpp

    # Cache
    core/cache/FileCache.cpp

    # Collision
    core/collision/CollisionSystem.cpp
    core/collision/spatial/GridIndex.cpp
    core/collision/queries/Raycast.cpp

    # Camera
    core/camera/camera.cpp
    core/camera/CameraController.cpp

    # Visibility
    core/visibility/BasicVisibility.cpp

    # Painter
    core/painter/painter.cpp
    core/painter/framebuffer/Framebuffer.cpp
    core/painter/framebuffer/DepthBuffer.cpp
    core/painter/material/MaterialSystem.cpp
    core/painter/texture/TextureSystem.cpp
    core/painter/texture/TextureSampler.cpp
    core/painter/lighting/LightSystem.cpp
    core/painter/sky/SkyRenderer.cpp
    core/painter/debug/DebugRenderer.cpp
    core/painter/rasterizer/VertexProcessor.cpp
    core/painter/rasterizer/Rasterizer.cpp
    core/painter/resources/MockResourceProvider.cpp

    # Scanner
    core/scanner/Scanner.cpp
    core/scanner/FileDiscovery.cpp
    core/scanner/HashService.cpp
    core/scanner/AssetRegistry.cpp
    core/scanner/DependencyResolver.cpp
    core/scanner/ScanProgress.cpp
    core/scanner/normalize/Infector.cpp
    core/scanner/analyzers/MeshAnalyzer.cpp
    core/scanner/analyzers/TextureAnalyzer.cpp
    core/scanner/analyzers/MetadataAnalyzer.cpp
    core/scanner/analyzers/BSAAnalyzer.cpp
    core/scanner/analyzers/ESPAnalyzer.cpp
    core/scanner/adapters/SkyrimXbox360Adapter.cpp
    core/scanner/adapters/SkyrimPCAdapter.cpp

    # Query — consulta por polígonos (CACHE -> REGIAO -> POLIGONOS -> AssetID -> ASSET -> MAPA MENTAL)
    core/query/RegionPolygonCache.cpp
    core/query/PolygonConsultant.cpp

    # Renderer (SDL2 parts)
    core/renderer/renderer.cpp
    core/renderer/SDL2Presenter.cpp

    # GPU Core (shared with emulator)
    core/gpu/Fsr10.cpp
    core/gpu/FramebufferOptimizer.cpp
    core/gpu/PainterCompute.cpp
    core/gpu/AssetPipeline.cpp
    core/gpu/Fsr2EasuCompute.cpp
    core/gpu/Fsr2TaaCompute.cpp
    core/gpu/Fsr2RcasCompute.cpp
    core/gpu/FrustumCullingCompute.cpp
    core/gpu/TileCache.cpp
    core/gpu/TileGrid.cpp
    core/gpu/DirtyRegionTracker.cpp
    core/gpu/MFOManager.cpp
    core/gpu/SeedPredictor.cpp
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

# Emulator GPU sources (compiled as part of mgd_core for tests)
# These are header-only except for Gpu.cpp and VulkanBackend.cpp
set(EMULATOR_GPU_SOURCES
    emulador-mgd/gpu/Gpu.cpp
    emulador-mgd/gpu/VulkanBackend.cpp
    emulador-mgd/runtime/MarioOdysseyRunner.cpp
)
target_sources(mgd_core PRIVATE ${EMULATOR_GPU_SOURCES})

# ========== MAIN EXECUTABLE ==========
if(SDL2_FOUND)
    add_executable(mgd_prototype src/main.cpp)
    target_link_libraries(mgd_prototype mgd_core)
    add_executable(mgd_launcher src/mgd_launcher.cpp)
    target_link_libraries(mgd_launcher mgd_core)
else()
    message(STATUS "SDL2 not found - mgd_prototype/mgd_launcher executables will not be built")
endif()

# ========== TESTS ==========
enable_testing()
add_executable(mgd_tests
    tests/test_main.cpp
    tests/test_types.cpp
    tests/test_entity.cpp
    tests/test_mental_map.cpp
    tests/test_camera.cpp
    tests/test_collision.cpp
    tests/test_visibility.cpp
    tests/test_framebuffer.cpp
    tests/test_rasterizer.cpp
    tests/test_painter.cpp
    tests/test_polygon_query.cpp
    tests/test_dna_pipeline.cpp
    tests/test_handoff_mock.cpp
    tests/test_shader_cache.cpp
    tests/test_pipeline_full.cpp
    tests/test_seed_provider.cpp
    tests/test_mmap_runtime.cpp
    tests/test_emulator_odyssey.cpp
    tests/test_emulator_machine.cpp
    tests/test_nca_decrypt.cpp
    tests/test_ipc_buffers.cpp
    tests/test_applet_service.cpp
)
target_link_libraries(mgd_tests mgd_core)
target_compile_definitions(mgd_tests PRIVATE MGD_TESTING)
add_test(NAME mgd_unit_tests COMMAND mgd_tests)

# Integration tests (requer build completo com GPU)
add_executable(mgd_tests_integration
    tests/test_integration_complete.cpp
)
target_link_libraries(mgd_tests_integration mgd_core Catch2::Catch2WithMain)
target_compile_definitions(mgd_tests_integration PRIVATE MGD_TESTING)
if(Vulkan_FOUND)
    target_link_libraries(mgd_tests_integration ${VULKAN_LIBRARIES})
endif()
add_test(NAME mgd_integration_tests COMMAND mgd_tests_integration)

# ========== BENCHMARK — consultador por polígonos ==========
add_executable(benchmark_polygon tools/benchmark_polygon.cpp)
target_link_libraries(benchmark_polygon mgd_core)

# ========== FERRAMENTA — inspetor NSP (só loaders, sem core) ==========
add_executable(mgd_nsp tools/mgd_nsp.cpp)
target_include_directories(mgd_nsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
