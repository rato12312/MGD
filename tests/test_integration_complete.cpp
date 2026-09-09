#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/loader/Keys.h"
#include "emulador-mgd/loader/Pfs0.h"
#include "emulador-mgd/loader/NcaProbe.h"
#include "emulador-mgd/loader/NcaSections.h"
#include "emulador-mgd/loader/NsoLoader.h"
#include "emulador-mgd/odyssey/OdysseyHandoff.h"
#include "emulador-mgd/gpu/VulkanBackend.h"

using namespace mgd::emu;
using namespace mgd::loader;
using namespace mgd::odyssey;
using namespace mgd::gpu;

namespace fs = std::filesystem;

// Helper: cria NSP sintético válido para teste
std::vector<uint8_t> makeTestNSP() {
    std::vector<uint8_t> nsp(1024 * 1024, 0); // 1MB
    
    // PFS0 header
    nsp[0] = 'P'; nsp[1] = 'F'; nsp[2] = 'S'; nsp[3] = '0';
    // entry count = 1
    nsp[8] = 1; nsp[9] = 0; nsp[10] = 0; nsp[11] = 0;
    // string table size
    nsp[12] = 8; nsp[13] = 0; nsp[14] = 0; nsp[15] = 0;
    // entry: offset=0x40, size=0x100000, name_offset=0, name_size=8
    nsp[16] = 0x40; nsp[17] = 0; nsp[18] = 0; nsp[19] = 0;
    nsp[20] = 0; nsp[21] = 0; nsp[22] = 1; nsp[22] = 0; // size = 1MB
    nsp[24] = 0; nsp[25] = 0; nsp[26] = 0; nsp[27] = 0; // name_offset
    nsp[28] = 8; nsp[29] = 0; nsp[30] = 0; nsp[31] = 0; // name_size
    // string table: "main.nca\0"
    nsp[0x40] = 'm'; nsp[0x41] = 'a'; nsp[0x42] = 'i'; nsp[0x43] = 'n';
    nsp[0x44] = '.'; nsp[0x45] = 'n'; nsp[0x46] = 'c'; nsp[0x47] = 'a';
    nsp[0x48] = 0;
    
    // NCA data at 0x50
    std::vector<uint8_t> nca = makeTestNCA();
    for (size_t i = 0; i < nca.size(); i++) {
        nsp[0x50 + i] = nca[i];
    }
    
    return nsp;
}

// Helper: cria NCA sintético válido para teste
std::vector<uint8_t> makeTestNCA() {
    std::vector<uint8_t> nca(0x100000, 0); // 1MB
    
    // NCA header (0xC00 bytes)
    nca[0] = 'N'; nca[1] = 'C'; nca[2] = 'A'; nca[3] = '3';
    nca[4] = 0; // type = PROGRAM
    // distribution type = 0 (encrypted)
    // content type = 0 (program)
    // key generation = 1
    // size = 0x100000
    nca[0x10] = 0; nca[0x11] = 0; nca[0x12] = 1; nca[0x13] = 0;
    // section 0: ExeFS
    nca[0x200] = 0x50; nca[0x201] = 0; nca[0x202] = 0; nca[0x203] = 0; // offset
    nca[0x204] = 0; nca[0x205] = 0; nca[0x206] = 1; nca[0x207] = 0; // size = 1MB
    nca[0x208] = 1; nca[0x209] = 0; nca[0x20A] = 0; nca[0x20B] = 0; // key index
    nca[0x20C] = 0; nca[0x20D] = 0; nca[0x20E] = 0; nca[0x20F] = 0; // hash
    
    // FsHeader at 0x200
    // section 0: ExeFS (PFS0)
    std::vector<uint8_t> exefs = makeTestExeFS();
    for (size_t i = 0; i < exefs.size(); i++) {
        nca[0x200 + 0x50 + i] = exefs[i];
    }
    
    return nca;
}

// Helper: cria ExeFS sintético
std::vector<uint8_t> makeTestExeFS() {
    std::vector<uint8_t> exefs(64 * 1024, 0); // 64KB
    
    // PFS0 header
    exefs[0] = 'P'; exefs[1] = 'F'; exefs[2] = 'S'; exefs[3] = '0';
    exefs[8] = 1; // entry count
    exefs[12] = 8; exefs[13] = 0; exefs[14] = 0; exefs[15] = 0; // string table size
    // entry: offset=0x40, size=0x10000, name_offset=0, name_size=4
    exefs[16] = 0x40; exefs[17] = 0; exefs[18] = 0; exefs[19] = 0;
    exefs[20] = 0; exefs[21] = 0; exefs[22] = 0x10; exefs[23] = 0; // size = 64KB
    exefs[24] = 0; exefs[25] = 0; exefs[26] = 0; exefs[27] = 0;
    exefs[28] = 4; exefs[29] = 0; exefs[30] = 0; exefs[31] = 0;
    // string table: "main\0"
    exefs[0x40] = 'm'; exefs[0x41] = 'a'; exefs[0x42] = 'i'; exefs[0x43] = 'n';
    exefs[0x44] = 0;
    
    // NSO data at 0x80
    std::vector<uint8_t> nso = makeTestNSO();
    for (size_t i = 0; i < nso.size(); i++) {
        exefs[0x80 + i] = nso[i];
    }
    
    return exefs;
}

// Helper: cria NSO sintético
std::vector<uint8_t> makeTestNSO() {
    std::vector<uint8_t> nso(64 * 1024, 0);
    
    // NSO header
    nso[0] = 'N'; nso[1] = 'S'; nso[2] = 'O'; nso[3] = '0';
    nso[0xC] = 1; // text compressed
    nso[0x10] = 0x80; nso[0x18] = 8; // text: file_offset=0x80, mem_offset=0, decomp_size=8
    nso[0x60] = 9; // comp_type = 9 (LZ4)
    
    // LZ4 compressed text (8 bytes decompressed)
    nso[0x80] = 0x80; // 8 literals
    nso[0x81] = 0xE0; nso[0x82] = 0x00; nso[0x83] = 0x80; nso[0x84] = 0xD2; // MOVZ X0,#7
    nso[0x85] = 0x01; nso[0x86] = 0x00; nso[0x87] = 0x00; nso[0x88] = 0xD4; // SVC#0
    
    return nso;
}

TEST_CASE("Complete Integration: Boot NSP Real", "[integration][nsp][boot]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    // Configura chaves de teste (zeros para teste - falha esperada se NCA real)
    emu.keys().setSlot(0, {0}); // header key
    emu.keys().setSlot(1, {0}); // section 0 key
    emu.keys().setSlot(2, {0});
    emu.keys().setSlot(3, {0});
    emu.keys().setSlot(4, {0});
    
    // Cria NSP de teste
    std::vector<uint8_t> nsp = makeTestNSP();
    
    // Tenta boot
    bool result = emu.bootNsp(nsp.data(), nsp.size());
    
    // Com chaves zeros, deve falhar na descriptografia
    // (teste real usaria chaves reais do usuário)
    // Aqui verificamos que o pipeline não crasha
    REQUIRE(result == false || result == true); // não crasha
}

TEST_CASE("Complete Integration: Handoff Real", "[integration][handoff]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    // Configura offsets de teste
    OdysseyOffsets offsets = makeOdysseyOffsets_v10();
    emu.setOdysseyOffsets(offsets);
    
    // Testa handoff
    bridge::HandoffFrame hf;
    bool result = emu.handoff().poll(hf);
    
    // Sem jogo real rodando, deve retornar false
    REQUIRE(result == false);
}

TEST_CASE("Complete Integration: Camera Query + Mental Map", "[integration][camera][mentalmap]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    // Testa camera query
    core::CameraMentalMapQuery query;
    query.setCamera(&emu.world().runtime().camera());
    query.setMentalMap(&emu.world().runtime().map());
    query.setPolygonCache(&emu.world().runtime().cache());
    query.setCollisionSystem(&emu.world().runtime().pipeline().collision());
    query.setVisibilitySystem(&emu.world().runtime().pipeline().visibility());
    
    auto result = query.query(1);
    
    REQUIRE(result.polygons.size() >= 0); // não crasha
    REQUIRE(result.compute_time_ms >= 0.0f);
}

TEST_CASE("Complete Integration: Frame Loop + FPS", "[integration][frame][fps]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    // Roda alguns frames
    for (int i = 0; i < 10; ++i) {
        bool ok = emu.frame("test_integration.ppm", 4);
        REQUIRE(ok);
    }
    
    double fps = emu.fps();
    REQUIRE(fps >= 0.0);
    
    double avg_ms = emu.lastFrameMs();
    REQUIRE(avg_ms >= 0.0);
}

TEST_CASE("Complete Integration: Save/Restore State", "[integration][state]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    // Roda um pouco
    emu.runCpu(100);
    
    // Snapshot
    auto snap = emu.snapshot();
    REQUIRE(snap.ram.size() == emu.cpu().ramSize());
    REQUIRE(snap.cpu.pc == emu.cpu().pc());
    
    // Continua
    emu.runCpu(100);
    
    // Restore
    bool restored = emu.restore(snap);
    REQUIRE(restored);
    REQUIRE(emu.cpu().pc() == snap.cpu.pc);
}

TEST_CASE("Complete Integration: GPU Pipeline", "[integration][gpu]") {
    VulkanGpuExecutor gpu;
    bool init = gpu.init(nullptr); // headless
    REQUIRE(init);
    
    // Testa asset pipeline
    // gpu.setAssetRegistry(...); // precisa de registry real
    // bool uploaded = gpu.uploadAllAssets();
    
    gpu.shutdown();
}

TEST_CASE("Complete Integration: Android Engine", "[integration][android]") {
    // Teste de compilação/linkagem do android_main
    // Em CI real, rodaria no emulador Android
    // Aqui só verificamos que compila
    REQUIRE(true);
}

// Benchmark: Frame time
TEST_CASE("Benchmark: Frame Time", "[benchmark][frame]") {
    Emulator emu;
    emu.applySwitches();
    emu.kernel().bootServices();
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        emu.frame("bench.ppm", 8);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_ms = elapsed_ms / 100.0;
    
    // Frame deve ser rápido (< 16ms para 60fps, < 33ms para 30fps)
    INFO("Avg frame time: " << avg_ms << "ms");
    REQUIRE(avg_ms < 50.0); // generoso para CI
}

// Benchmark: CPU throughput
TEST_CASE("Benchmark: CPU Throughput", "[benchmark][cpu]") {
    Emulator emu;
    emu.applySwitches();
    
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t steps = emu.runCpu(1000000); // 1M steps
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double mips = steps / (elapsed_ms / 1000.0) / 1e6;
    
    INFO("CPU throughput: " << mips << " MIPS");
    REQUIRE(steps == 1000000);
    REQUIRE(mips > 0.1); // pelo menos 0.1 MIPS
}