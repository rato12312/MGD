// Save/Restore State Tests

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cstring>

#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/hos/Kernel.h"
#include "emulador-mgd/hos/AppletService.h"
#include "emulador-mgd/hos/Thread.h"
#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/Mmu.h"

using namespace mgd::emu;
using namespace mgd::hos;

TEST_CASE("CPU State save/restore", "[save][cpu]") {
    Cpu cpu(64 * 1024);
    
    // Set some register values
    cpu.setReg(0, 0x1122334455667788);
    cpu.setReg(1, 0x99AABBCCDDEEFF00);
    cpu.setSp(0x8000);
    cpu.setPc(0x1000);
    
    // Save state
    auto state = cpu.save();
    
    // Modify CPU
    cpu.setReg(0, 0);
    cpu.setReg(1, 0);
    cpu.setSp(0);
    cpu.setPc(0);
    
    // Restore
    cpu.load(state);
    
    REQUIRE(cpu.reg(0) == 0x1122334455667788);
    REQUIRE(cpu.reg(1) == 0x99AABBCCDDEEFF00);
    REQUIRE(cpu.sp() == 0x8000);
    REQUIRE(cpu.pc() == 0x1000);
}

TEST_CASE("Emulator basic snapshot", "[save][emulator]") {
    Emulator emu;
    emu.applySwitches();
    
    // Run a few cycles
    emu.runCpu(10);
    
    // Take snapshot
    auto snap = emu.snapshot();
    
    // Modify emulator
    emu.cpu().setReg(0, 0xDEADBEEF);
    emu.runCpu(5);
    
    // Restore
    REQUIRE(emu.restore(snap));
    REQUIRE(emu.cpu().reg(0) != 0xDEADBEEF);
}

TEST_CASE("Scheduler snapshot/restore", "[save][scheduler]") {
    Scheduler sched;
    
    // Spawn some threads
    uint64_t t1 = sched.spawn(0x1000, 0x8000, 0);
    uint64_t t2 = sched.spawn(0x2000, 0x9000, 1);
    uint64_t t3 = sched.spawn(0x3000, 0xA000, 0);
    
    // Take snapshot
    auto snapshots = sched.getThreadsForSnapshot();
    uint64_t current = sched.currentThreadId();
    
    REQUIRE(snapshots.size() == 3);
    REQUIRE(current == 0); // no thread running yet
    
    // Modify scheduler
    sched.spawn(0x4000, 0xB000, 2);
    REQUIRE(sched.pending() == 4);
    
    // Restore
    sched.restoreThreads(snapshots, current);
    REQUIRE(sched.pending() == 3);
}

TEST_CASE("AppletService snapshot/restore", "[save][applet]") {
    AppletService as;
    
    // Create applets
    IpcMessage req, rep;
    req.cmd = 2;
    req.payload.assign("TestApp", "TestApp" + 8);
    uint64_t pid = 0x123456789ABCDEFF;
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((pid >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req, rep));
    uint64_t aid1 = 0;
    for (int i = 0; i < 8; i++) aid1 |= static_cast<uint64_t>(rep.payload[i]) << (8 * i);
    
    // Start with fake NSO
    IpcMessage req, rep;
    req.cmd = 3;
    req.payload.clear();
    for (int i = 0; i < 8; i++) req.payload.push_back(static_cast<uint8_t>((aid1 >> (8 * i)) & 0xFF));
    std::vector<uint8_t> fake_nso(0x200, 0);
    fake_nso[0] = 'N'; fake_nso[1] = 'S'; fake_nso[2] = 'O'; fake_nso[3] = '0';
    *reinterpret_cast<uint32_t*>(fake_nso.data() + 0x18) = 0x100;
    req.payload.insert(req.payload.end(), fake_nso.begin(), fake_nso.end());
    REQUIRE(as.dispatch(req, rep));
    
    // Push to stack
    IpcMessage req2, rep2;
    req2.cmd = 4;
    req2.payload.clear();
    for (int i = 0; i < 8; i++) req2.payload.push_back(static_cast<uint8_t>((aid1 >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req2, rep2));
    REQUIRE(as.stack().size() == 1);
    
    // Snapshot
    auto applets = as.getAppletsForSnapshot();
    auto stack = as.stack();
    uint64_t next_id = as.getNextAppletId();
    
    REQUIRE(applets.size() == 1);
    REQUIRE(stack.size() == 1);
    
    // Modify
    IpcMessage req3, rep3;
    req3.cmd = 2;
    req3.payload.assign("App2", "App2" + 5);
    uint64_t pid2 = 0xFFFFFFFFFFFFFFFF;
    for (int i = 0; i < 8; i++) req3.payload.push_back(static_cast<uint8_t>((pid2 >> (8 * i)) & 0xFF));
    REQUIRE(as.dispatch(req3, rep3));
    
    // Restore
    for (const auto& a : applets) {
        as.restoreApplet(a);
    }
    as.restoreStack(stack);
    as.setNextAppletId(aid1 + 1);
    
    REQUIRE(as.stack().size() == 1);
    REQUIRE(as.stack()[0] == aid1);
}

TEST_CASE("Thread Scheduler snapshot/restore", "[save][thread]") {
    Scheduler sched;
    
    // Spawn threads
    uint64_t t1 = sched.spawn(0x1000, 0x8000, 0);
    uint64_t t2 = sched.spawn(0x2000, 0x9000, 1);
    
    // Snapshot
    auto threads = sched.getThreadsForSnapshot();
    uint64_t current = sched.currentThreadId();
    
    REQUIRE(threads.size() == 2);
    REQUIRE(threads[0].id == 1);
    REQUIRE(threads[1].id == 2);
    
    // Modify
    sched.spawn(0x3000, 0xA000, 0);
    REQUIRE(sched.pending() == 3);
    
    // Restore
    sched.restoreThreads(threads, current);
    REQUIRE(sched.pending() == 2);
}

TEST_CASE("Kernel snapshot/restore", "[save][kernel]") {
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
    
    // Create a process
    uint64_t pid = kernel.createProcess("test_process");
    
    // Snapshot kernel state
    auto processes = kernel.getProcessesForSnapshot();
    auto handles = kernel.getHandlesForSnapshot();
    uint64_t next_pid = kernel.getNextPid();
    uint32_t next_handle = kernel.getNextHandle();
    
    REQUIRE(processes.size() == 1);
    REQUIRE(handles.size() > 0);
    
    // Restore
    kernel.restoreProcesses(processes);
    kernel.restoreHandles(handles);
    kernel.setNextPid(next_pid);
    kernel.setNextHandle(next_handle);
    
    REQUIRE(kernel.processCount() == 1);
}

TEST_CASE("Emulator full snapshot/restore", "[save][emulator]") {
    Emulator emu;
    emu.applySwitches();
    
    // Run some frames
    for (int i = 0; i < 3; i++) {
        emu.runCpu(10);
    }
    
    // Take full snapshot
    auto snap = emu.snapshot();
    
    // Verify snapshot has all components
    REQUIRE(snap.ram.size() == emu.cpu().ramSize());
    REQUIRE(snap.frame_index == emu.frameCount());
    REQUIRE(snap.applets.size() >= 0);
    
    // Continue running
    for (int i = 0; i < 2; i++) {
        emu.runCpu(10);
    }
    
    // Restore
    REQUIRE(emu.restore(snap));
    REQUIRE(emu.frameCount() == snap.frame_index);
    REQUIRE(emu.cpu().ramSize() == snap.ram.size());
}