// AppletService Tests

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cstring>

#include "emulador-mgd/hos/AppletService.h"
#include "emulador-mgd/hos/Kernel.h"
#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/Mmu.h"
#include "emulador-mgd/loader/NsoLoader.h"

using namespace mgd::emu;
using namespace mgd::hos;

TEST_CASE("AppletService Create/Start", "[applet]") {
    AppletService as;
    
    // Create applet
    IpcMessage req, rep;
    req.cmd = 2; // CreateApplet
    req.payload.assign("TestApplet", "TestApplet" + 11);
    uint64_t pid = 0x123456789ABCDEFF;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 8);
    
    uint64_t aid = 0;
    for (int i = 0; i < 8; i++) aid |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    REQUIRE(aid != 0);
    
    // Start applet (needs NSO blob)
    req.cmd = 3; // StartApplet
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
    // Add fake NSO blob (minimal)
    std::vector<uint8_t> fake_nso(0x200, 0);
    fake_nso[0] = 'N'; fake_nso[1] = 'S'; fake_nso[2] = 'O'; fake_nso[3] = '0';
    *reinterpret_cast<uint32_t*>(fake_nso.data() + 0x18) = 0x100; // entry offset
    req.payload.insert(req.payload.end(), fake_nso.begin(), fake_nso.end());
    
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
}

TEST_CASE("AppletService Push/Pop", "[applet]") {
    AppletService as;
    
    // Create two applets
    IpcMessage req, rep;
    req.cmd = 2;
    req.payload.assign("App1", "App1" + 5);
    uint64_t pid = 0x1111111111111111;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    uint64_t aid1 = 0;
    for (int i = 0; i < 8; i++) aid1 |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    
    req.cmd = 2;
    req.payload.assign("App2", "App2" + 5);
    pid = 0x2222222222222222;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    uint64_t aid2 = 0;
    for (int i = 0; i < 8; i++) aid2 |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    
    // Push app1
    req.cmd = 4;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid1 >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(as.stack().size() == 1);
    REQUIRE(as.stack()[0] == aid1);
    
    // Push app2
    req.cmd = 4;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid2 >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(as.stack().size() == 2);
    REQUIRE(as.stack()[1] == aid2);
    
    // Pop app2
    req.cmd = 5;
    req.payload.clear();
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(as.stack().size() == 1);
    REQUIRE(as.stack()[0] == aid1);
    
    // Pop app1
    req.cmd = 5;
    req.payload.clear();
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(as.stack().empty());
}

TEST_CASE("AppletService State queries", "[applet]") {
    AppletService as;
    
    // Create applet
    IpcMessage req, rep;
    req.cmd = 2;
    req.payload.assign("TestApp", "TestApp" + 8);
    uint64_t pid = 0x123456789ABCDEFF;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    
    uint64_t aid = 0;
    for (int i = 0; i < 8; i++) aid |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    
    // Get state (CREATED)
    req.cmd = 6;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 1);
    REQUIRE(rep.payload[0] == static_cast<uint8_t>(AppletState::CREATED));
    
    // Start with fake NSO
    req.cmd = 3;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
    std::vector<uint8_t> fake_nso(0x200, 0);
    fake_nso[0] = 'N'; fake_nso[1] = 'S'; fake_nso[2] = 'O'; fake_nso[3] = '0';
    *reinterpret_cast<uint32_t*>(fake_nso.data() + 0x18) = 0x100;
    req.payload.insert(req.payload.end(), fake_nso.begin(), fake_nso.end());
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    
    // Get state (RUNNING)
    req.cmd = 6;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload[0] == static_cast<uint8_t>(AppletState::RUNNING));
    
    // NotifyRunning
    req.cmd = 7;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
}

TEST_CASE("AppletService GetAppletResourceUserId", "[applet]") {
    AppletService as;
    
    IpcMessage req, rep;
    req.cmd = 1;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 8);
    REQUIRE(rep.payload[0] == 1); // self
}

TEST_CASE("AppletService SharedFont", "[applet]") {
    AppletService as;
    
    IpcMessage req, rep;
    
    // GetSharedFontSharedMemoryHandle
    req.cmd = 8;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 4);
    REQUIRE(rep.payload[0] == 1); // handle
    
    // GetSharedFontInlines
    req.cmd = 9;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    
    // GetLibraryAppletCreator
    req.cmd = 10;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
}

TEST_CASE("AppletService OperationMode and Register", "[applet]") {
    AppletService as;
    
    IpcMessage req, rep;
    
    // GetAppletOperationMode
    req.cmd = 11;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 8);
    REQUIRE(rep.payload[0] == 0); // normal mode
    
    // RegisterAppletResourceUserId
    req.cmd = 12;
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    REQUIRE(rep.payload.size() == 1);
    REQUIRE(rep.payload[0] == 0);
}

TEST_CASE("AppletService GetApplet accessor", "[applet]") {
    AppletService as;
    
    // Create applet
    IpcMessage req, rep;
    req.cmd = 2;
    req.payload.assign("AccessorTest", "AccessorTest" + 13);
    uint64_t pid = 0x5555555555555555;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    REQUIRE(rep.cmd == 1);
    
    uint64_t aid = 0;
    for (int i = 0; i < 8; i++) aid |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    
    // Access applet
    const Applet* a = as.getApplet(aid);
    REQUIRE(a != nullptr);
    REQUIRE(a->id == aid);
    REQUIRE(a->name == "AccessorTest");
    REQUIRE(a->program_id == pid);
    REQUIRE(a->state == AppletState::CREATED);
    
    // Non-existent
    REQUIRE(as.getApplet(0xFFFFFFFFFFFFFFFF) == nullptr);
}

TEST_CASE("AppletService Stack operations", "[applet]") {
    AppletService as;
    
    // Create 3 applets
    std::vector<uint64_t> aids;
    for (int n = 0; n < 3; n++) {
        IpcMessage req, rep;
        req.cmd = 2;
        req.payload.assign("App" + std::to_string(n), std::string("App" + std::to_string(n)).length() + 1);
        uint64_t pid = 0xAAAAAAAAAAAAAAAA + n;
        for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
        REQUIRE(as.dispatch(req, rep));
        REQUIRE(rep.cmd == 1);
        uint64_t aid = 0;
        for (int i = 0; i < 8; i++) aid |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
        aids.push_back(aid);
    }
    
    // Push all
    for (auto aid : aids) {
        IpcMessage req, rep;
        req.cmd = 4;
        req.payload.clear();
        for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid >> (8 * i)) & 0xFF));
        REQUIRE(as.dispatch(req, rep));
        REQUIRE(rep.cmd == 1);
    }
    
    REQUIRE(as.stack().size() == 3);
    REQUIRE(as.stack()[0] == aids[0]);
    REQUIRE(as.stack()[1] == aids[1]);
    REQUIRE(as.stack()[2] == aids[2]);
    
    // Pop all
    for (int i = 2; i >= 0; i--) {
        IpcMessage req, rep;
        req.cmd = 5;
        req.payload.clear();
        REQUIRE(as.dispatch(req, rep));
        REQUIRE(rep.cmd == 1);
        REQUIRE(as.stack().size() == i);
    }
    
    REQUIRE(as.stack().empty());
}

TEST_CASE("Kernel Applet boot integration", "[applet][kernel]") {
    Cpu cpu(64 * 1024);
    Mmu mmu;
    Kernel kernel;
    
    kernel.setMmu(&mmu);
    kernel.setRam(cpu.ram(), cpu.ramSize());
    mmu.map(0x10000000, 0x10000000, 0x10000, true, true, true);
    cpu.setMmu(&mmu);
    cpu.setPc(0x10000000);
    
    // Boot services
    kernel.bootServices();
    
    // Verify appletOE is published
    auto probe = kernel.services().findPort("appletOE");
    REQUIRE(probe == 1);
}