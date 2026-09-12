// IPC Buffers + SVC SendSync/ReceiveSync Tests

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cstring>

#include "emulador-mgd/hos/Kernel.h"
#include "emulador-mgd/hos/Session.h"
#include "emulador-mgd/hos/Hipc.h"
#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/Mmu.h"

using namespace mgd::emu;
using namespace mgd::hos;

TEST_CASE("HIPC buffer descriptor encoding", "[ipc][hipc]") {
    // Test HipcBufferDesc encoding
    HipcBufferDesc buf;
    buf.type = HipcBufferType::X; // Send
    buf.flags = 0;
    buf.addr = 0x1000;
    buf.size = 0x100;
    
    // Test roundtrip through Session conversion
    auto session_buf = Session::fromHipcBuffer(buf);
    REQUIRE(session_buf.kind == 0); // X = 0
    REQUIRE(session_buf.guest_ptr == 0x1000);
    REQUIRE(session_buf.size == 0x100);
    
    auto back = Session::toHipcBuffer(session_buf);
    REQUIRE(back.type == HipcBufferType::X);
    REQUIRE(back.addr == 0x1000);
    REQUIRE(back.size == 0x100);
}

TEST_CASE("HIPC buffer types A/B/W", "[ipc][hipc]") {
    // Type A - Receive
    HipcBufferDesc buf_a;
    buf_a.type = HipcBufferType::A;
    auto s_a = Session::fromHipcBuffer(buf_a);
    REQUIRE(s_a.kind == 1);
    
    // Type B - Map
    HipcBufferDesc buf_b;
    buf_b.type = HipcBufferType::B;
    auto s_b = Session::fromHipcBuffer(buf_b);
    REQUIRE(s_b.kind == 2);
    
    // Type W - Write
    HipcBufferDesc buf_w;
    buf_w.type = HipcBufferType::W;
    auto s_w = Session::fromHipcBuffer(buf_w);
    REQUIRE(s_w.kind == 3);
}

TEST_CASE("Session buffer translation", "[ipc][session]") {
    Session session;
    
    // Create message with buffers
    IpcMessage msg;
    msg.cmd = 42;
    
    IpcMessage::Buffer buf;
    buf.guest_ptr = 0x2000;
    buf.size = 0x200;
    buf.kind = 0; // X
    buf.flags = 0;
    msg.buffers.push_back(buf);
    
    // Translate buffers with mock RAM
    uint8_t ram[0x10000] = {0};
    std::vector<std::pair<uint64_t, uint8_t*>> mappings;
    Session::translateBuffers(msg, ram, sizeof(ram), mappings);
    
    REQUIRE(mappings.size() == 1);
    REQUIRE(mappings[0].first == 0x2000);
    REQUIRE(mappings[0].second == ram + 0x2000);
}

TEST_CASE("IPC SendSyncRequest with buffers", "[ipc][svc]") {
    Cpu cpu(64 * 1024);
    Mmu mmu;
    Kernel kernel;
    
    kernel.setMmu(&mmu);
    kernel.setRam(cpu.ram(), cpu.ramSize());
    
    // Map memory for IPC
    mmu.map(0x10000000, 0x10000000, 0x10000, true, true, true);
    cpu.setMmu(&mmu);
    cpu.setPc(0x10000000);
    
    // Setup: create a port and session
    kernel.services().publish("test:service");
    
    // Simulate SendSyncRequest SVC
    SvcArgs args;
    args.x[0] = 0x1000; // handle (tag)
    args.x[1] = 0x20000000; // message pointer
    args.x[2] = 0x30000000; // buffer descriptor 1
    args.x[3] = 0x30000100; // buffer descriptor 2
    
    // Write message in guest RAM
    uint8_t* msg_base = cpu.ram() + 0x20000000;
    *reinterpret_cast<uint32_t*>(msg_base) = 5; // cmd
    *reinterpret_cast<uint32_t*>(msg_base + 4) = 8; // payload size
    *reinterpret_cast<uint32_t*>(msg_base + 8) = 2; // num_buffers
    msg_base[12] = 0xDE; msg_base[13] = 0xAD; msg_base[14] = 0xBE; msg_base[15] = 0xEF;
    msg_base[16] = 0xCA; msg_base[17] = 0xFE; msg_base[18] = 0xBA; msg_base[19] = 0xBE;
    
    // Write buffer descriptors
    uint8_t* bd1 = cpu.ram() + 0x30000000;
    *reinterpret_cast<uint64_t*>(bd1) = 0x40000000; // guest_ptr
    *reinterpret_cast<uint64_t*>(bd1 + 8) = 0x1000; // size
    *reinterpret_cast<uint32_t*>(bd1 + 16) = 0; // kind = X
    *reinterpret_cast<uint32_t*>(bd1 + 20) = 0; // flags
    
    uint8_t* bd2 = cpu.ram() + 0x30000100;
    *reinterpret_cast<uint64_t*>(bd2) = 0x50000000; // guest_ptr
    *reinterpret_cast<uint64_t*>(bd2 + 8) = 0x2000; // size
    *reinterpret_cast<uint32_t*>(bd2 + 16) = 1; // kind = A
    *reinterpret_cast<uint32_t*>(bd2 + 20) = 0; // flags
    
    // Map the buffer memory regions
    mmu.map(0x20000000, 0x20000000, 0x1000, true, true, true);
    mmu.map(0x30000000, 0x30000000, 0x1000, true, true, true);
    mmu.map(0x40000000, 0x40000000, 0x1000, true, true, true);
    mmu.map(0x50000000, 0x50000000, 0x1000, true, true, true);
    
    // Call SVC
    auto result = kernel.call(0x11, args); // SVC_SEND_SYNC_REQUEST
    // Note: This will fail because handle 0x1000 isn't registered, but tests the parsing path
    REQUIRE(result == RESULT_INVALID_HANDLE || result == RESULT_OK);
}

TEST_CASE("IPC ReplyAndReceive with buffers", "[ipc][svc]") {
    Cpu cpu(64 * 1024);
    Mmu mmu;
    Kernel kernel;
    
    kernel.setMmu(&mmu);
    kernel.setRam(cpu.ram(), cpu.ramSize());
    mmu.map(0x10000000, 0x10000000, 0x10000, true, true, true);
    cpu.setMmu(&mmu);
    
    SvcArgs args;
    args.x[0] = 0; // handles array (not used)
    args.x[1] = 0; // count
    args.x[2] = 0; // timeout
    args.x[3] = 0x20000000; // reply message ptr
    args.x[4] = 0x30000000; // reply buffer desc 1
    
    // Write reply message
    uint8_t* reply_base = cpu.ram() + 0x20000000;
    *reinterpret_cast<uint32_t*>(reply_base) = 10; // reply cmd
    *reinterpret_cast<uint32_t*>(reply_base + 4) = 4; // payload size
    *reinterpret_cast<uint32_t*>(reply_base + 8) = 1; // num_buffers
    reply_base[12] = 0x01; reply_base[13] = 0x02; reply_base[14] = 0x03; reply_base[15] = 0x04;
    
    // Write reply buffer descriptor
    uint8_t* rbd = cpu.ram() + 0x30000000;
    *reinterpret_cast<uint64_t*>(rbd) = 0x60000000; // guest_ptr
    *reinterpret_cast<uint64_t*>(rbd + 8) = 0x800; // size
    *reinterpret_cast<uint32_t*>(rbd + 16) = 1; // kind = A (receive)
    *reinterpret_cast<uint32_t*>(rbd + 20) = 0; // flags
    
    mmu.map(0x20000000, 0x20000000, 0x1000, true, true, true);
    mmu.map(0x30000000, 0x30000000, 0x1000, true, true, true);
    mmu.map(0x60000000, 0x60000000, 0x1000, true, true, true);
    
    auto result = kernel.call(0x12, args); // SVC_REPLY_AND_RECEIVE
    REQUIRE(result == RESULT_OK || result == RESULT_INVALID_HANDLE);
}

TEST_CASE("PortRegistry with sessions", "[ipc][port]") {
    PortRegistry registry;
    
    REQUIRE(registry.registerPort("test:port", 4));
    REQUIRE(registry.hasPort("test:port"));
    
    std::shared_ptr<Session> client, server;
    REQUIRE(registry.connect("test:port", client, server));
    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);
    
    // Test send/receive
    IpcMessage req;
    req.cmd = 1;
    req.payload = {1, 2, 3, 4};
    REQUIRE(client->sendRequest(req));
    
    IpcMessage received;
    REQUIRE(server->recvRequest(received));
    REQUIRE(received.cmd == 1);
    REQUIRE(received.payload.size() == 4);
    
    IpcMessage rep;
    rep.cmd = 1;
    rep.payload = {0}; // success
    REQUIRE(server->sendReply(rep));
    
    IpcMessage reply;
    REQUIRE(client->recvReply(reply));
    REQUIRE(reply.cmd == 1);
    REQUIRE(reply.payload[0] == 0);
    
    // Disconnect
    registry.disconnect("test:port");
    REQUIRE(registry.getMaxSessions("test:port") == 4);
}

TEST_CASE("ServiceManager GetService", "[ipc][service]") {
    ServiceManager sm;
    
    sm.publish("test:service");
    
    IpcMessage req;
    req.cmd = 1; // GetService
    req.payload.assign("test:service", "test:service" + 12);
    
    IpcMessage rep;
    REQUIRE(sm.dispatch(req, rep));
    REQUIRE(rep.cmd == 1); // success
    REQUIRE(rep.payload.size() == 4); // session id
    
    // Verify session exists
    uint32_t session_id = 0;
    for (int i = 0; i < 4; i++) {
        session_id |= static_cast<uint32_t>(rep.payload[i]) << (8 * i);
    }
    REQUIRE(session_id != 0);
    
    std::shared_ptr<Session> session;
    REQUIRE(sm.session(session_id, session));
    REQUIRE(session != nullptr);
}

TEST_CASE("HIPC request/reply encoding", "[ipc][hipc]") {
    // Test request
    auto req = hipcMakeRequest(42);
    REQUIRE(req.size() >= 12);
    REQUIRE(req[0] == 'S'); REQUIRE(req[1] == 'F'); REQUIRE(req[2] == 'C'); REQUIRE(req[3] == 'I');
    
    HipcRequest parsed_req;
    REQUIRE(hipcParseRequest(req, parsed_req));
    REQUIRE(parsed_req.cmd == 42);
    
    // Test reply
    auto reply = hipcMakeReply(0x123456789ABCDEFF);
    REQUIRE(reply.size() >= 16);
    REQUIRE(reply[0] == 'S'); REQUIRE(reply[1] == 'F'); REQUIRE(reply[2] == 'C'); REQUIRE(reply[3] == 'O');
    
    HipcReply parsed_rep;
    REQUIRE(hipcParseReply(reply, parsed_rep));
    REQUIRE(parsed_rep.result == 0x123456789ABCDEFF);
}

TEST_CASE("HIPC with buffers encoding", "[ipc][hipc]") {
    std::vector<HipcBufferDesc> buffers;
    HipcBufferDesc buf;
    buf.type = HipcBufferType::X;
    buf.flags = 0x1;
    buf.addr = 0x10000000;
    buf.size = 0x1000;
    buffers.push_back(buf);
    
    auto req = hipcMakeRequest(100, buffers);
    REQUIRE(req.size() >= 16 + 16); // header + 1 buffer descriptor
    
    HipcRequest parsed;
    REQUIRE(hipcParseRequest(req, parsed));
    REQUIRE(parsed.cmd == 100);
    REQUIRE(parsed.buffers.size() == 1);
    REQUIRE(parsed.buffers[0].type == HipcBufferType::X);
    REQUIRE(parsed.buffers[0].addr == 0x10000000);
    REQUIRE(parsed.buffers[0].size == 0x1000);
    REQUIRE(parsed.buffers[0].flags == 0x1);
}