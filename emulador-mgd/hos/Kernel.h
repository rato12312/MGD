#pragma once

// Kernel HOS mínimo: tabela SVC + resultados.
// Cada chamada real cresce aqui. Desconhecida = erro, nunca trava.

#include <cstdint>

#include <string>
#include <unordered_map>

#include "../ram/Mmu.h"
#include "ApmService.h"
#include "AppletService.h"
#include "AccService.h"
#include "AudService.h"
#include "FatalService.h"
#include "FsService.h"
#include "HidService.h"
#include "LblService.h"
#include "PmService.h"
#include "PsmService.h"
#include "SetService.h"
#include "TimeService.h"
#include "NvService.h"
#include "ViService.h"
#include "ServiceManager.h"
#include "Thread.h"
#include "Session.h"
#include "Mutex.h"
#include "Event.h"

namespace mgd {
namespace hos {

// Números SVC do Horizon OS (subset inicial).
enum SvcNumber : uint32_t {
    SVC_SET_HEAP_SIZE = 0x01,
    SVC_SET_MEMORY_PERMISSION = 0x02, // X0=addr X1=size X2=perm(rwx bits)
    SVC_SET_MEMORY_ATTRIBUTE = 0x03,  // X0=addr X1=size X2=mask X3=attr (guarda)
    SVC_MAP_MEMORY = 0x04,            // X0=dst X1=src X2=size
    SVC_UNMAP_MEMORY = 0x05,          // X0=addr X1=size (match exato)
    SVC_QUERY_MEMORY = 0x06,
    SVC_EXIT_PROCESS = 0x07,
    SVC_CREATE_THREAD = 0x08, // simplificada: X1=entry, X2=sp
    SVC_SLEEP_THREAD = 0x0B,  // número a confirmar contra TRM; semântica: X0=ns
    SVC_GET_INFO = 0x29,
};

// Estados de memória (resumo honesto do HOS).
enum MemState : uint32_t {
    MEM_UNMAPPED = 0x00,
    MEM_NORMAL = 0x03,
};

// Layout que escrevemos no guest: base(0) size(8) state(16) perm(20).
// perm: bit0=r bit1=w bit2=x.
struct MemInfo {
    uint64_t base = 0;
    uint64_t size = 0;
    uint32_t state = MEM_UNMAPPED;
    uint32_t perm = 0;
};

// Resultados (códigos reais do HOS, resumidos).
enum SvcResult : uint32_t {
    RESULT_OK = 0x0,
    RESULT_UNIMPLEMENTED = 0xD401, // nosso: ainda não existe
    RESULT_INVALID_HANDLE = 0xE401,
};

struct SvcArgs {
    uint64_t x[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // X0-X7 na entrada
    uint64_t out[2] = {0, 0};                 // X0-X1 na saída
};

struct Process {
    uint64_t pid = 0;
    std::string name;
};

class Kernel : public emu::SvcHost {
public:
    Kernel() = default;

    uint64_t createProcess(const std::string& name) {
        uint64_t pid = next_pid_++;
        processes_[pid] = Process{pid, name};
        return pid;
    }
    bool getProcess(uint64_t pid, Process& out) const {
        auto it = processes_.find(pid);
        if (it == processes_.end()) return false;
        out = it->second;
        return true;
    }
    bool killProcess(uint64_t pid) { return processes_.erase(pid) > 0; }
    size_t processCount() const { return processes_.size(); }

    // Handles: id opaco -> etiqueta (sessão, porta, thread...).
    uint32_t createHandle(uint32_t tag) {
        uint32_t h = next_handle_++;
        handles_[h] = tag;
        return h;
    }
    bool getHandle(uint32_t h, uint32_t& tag) const {
        auto it = handles_.find(h);
        if (it == handles_.end()) return false;
        tag = it->second;
        return true;
    }
    bool closeHandle(uint32_t h) { return handles_.erase(h) > 0; }
    size_t handleCount() const { return handles_.size(); }

    void setMmu(emu::Mmu* mmu) { mmu_ = mmu; }
    void setRam(uint8_t* ram, uint64_t size) { ram_ = ram; ram_size_ = size; }

    uint32_t svcCall(uint32_t num, uint64_t x[8], uint64_t out[2]) override {
        SvcArgs args;
        for (int i = 0; i < 8; i++) args.x[i] = x[i];
        SvcResult r = call(num, args);
        out[0] = args.out[0];
        out[1] = args.out[1];
        return static_cast<uint32_t>(r);
    }
    bool svcExited() const override { return exited_; }

    uint64_t heapBase() const { return heap_base_; }
    uint64_t heapSize() const { return heap_size_; }
    uint64_t lastMemAttr() const { return last_mem_attr_; }
    bool exited() const { return exited_; }
    uint64_t sleptNs() const { return slept_ns_; }
    Scheduler& scheduler() { return sched_; }
    ServiceManager& services() { return services_; }
    uint64_t runThreads(emu::Cpu& cpu, uint64_t maxSteps, uint64_t quantum = 4) {
        return sched_.run(cpu, maxSteps, quantum);
    }

    // Boot publica os serviços que o jogo procura primeiro.
    void bootServices() {
        services_.publish("nvdrv:a"); // GPU (stub, comandos vêm depois)
        services_.publish("vi:u");    // vídeo/display
        services_.publish("audren:u"); // áudio render
        services_.publish("fsp-srv"); // filesystem
        services_.publish("hid:u");   // input
        services_.publish("time:u");  // relógio
        services_.publish("apm");     // performance (handheld)
        services_.publish("psm");     // bateria
        services_.publish("acc:u");   // conta
        services_.publish("appletOE"); // applet manager
        services_.publish("lbl:u");   // brilho
        services_.publish("set:sys"); // idioma/região
        services_.publish("fatal:u"); // erros registrados
        services_.publish("pm:dmnt"); // processos
    }

    // Bomba: um pedido pendente por sessão anda até o serviço dono.
    // Retorna quantos pedidos foram atendidos.
    uint32_t pumpServices() {
        uint32_t done = 0;
        for (const auto& kv : services_.allSessions()) {
            std::string name = services_.serviceOf(kv.first);
            IpcMessage req;
            if (!kv.second->recvRequest(req)) continue;
            IpcMessage rep;
            bool understood = false;
            if (name == "nvdrv:a") understood = nv_.dispatch(req, rep);
            else if (name == "vi:u") understood = vi_.dispatch(req, rep);
            else if (name == "audren:u") understood = aud_.dispatch(req, rep);
            else if (name == "fsp-srv") understood = fs_.dispatch(req, rep);
            else if (name == "hid:u") understood = hid_.dispatch(req, rep);
            else if (name == "time:u") understood = time_.dispatch(req, rep);
            else if (name == "apm") understood = apm_.dispatch(req, rep);
            else if (name == "acc:u") understood = acc_.dispatch(req, rep);
            else if (name == "appletOE") understood = applet_.dispatch(req, rep);
            else if (name == "psm") understood = psm_.dispatch(req, rep);
            else if (name == "lbl:u") understood = lbl_.dispatch(req, rep);
            else if (name == "set:sys") understood = set_.dispatch(req, rep);
            else if (name == "fatal:u") understood = fatal_.dispatch(req, rep);
            else if (name == "pm:dmnt") understood = pm_.dispatch(req, rep);
            if (understood && kv.second->sendReply(rep)) done++;
        }
        return done;
    }
    NvService& nv() { return nv_; }
    ViService& vi() { return vi_; }
    AudService& aud() { return aud_; }
    FsService& fs() { return fs_; }
    HidService& hid() { return hid_; }
    TimeService& time() { return time_; }
    ApmService& apm() { return apm_; }
    AccService& acc() { return acc_; }
    AppletService& applet() { return applet_; }
    PsmService& psm() { return psm_; }
    LblService& lbl() { return lbl_; }
    SetService& set() { return set_; }
    FatalService& fatal() { return fatal_; }
    PmService& pm() { return pm_; }

    SvcResult call(uint32_t num, SvcArgs& args) {
        switch (num) {
            case SVC_SET_HEAP_SIZE: {
                heap_size_ = args.x[1];
                args.out[0] = heap_base_;
                if (mmu_ && heap_size_ > 0) mmu_->map(heap_base_, heap_base_, heap_size_, true, true, false);
                return RESULT_OK;
            }
            case SVC_SET_MEMORY_PERMISSION: {
                if (!mmu_) return RESULT_INVALID_HANDLE;
                uint32_t p = static_cast<uint32_t>(args.x[2]);
                bool ok = mmu_->protect(args.x[0], args.x[1],
                                        (p & 1) != 0, (p & 2) != 0, (p & 4) != 0);
                return ok ? RESULT_OK : RESULT_INVALID_HANDLE;
            }
            case SVC_SET_MEMORY_ATTRIBUTE: {
                last_mem_attr_ = args.x[3];
                return RESULT_OK;
            }
            case SVC_MAP_MEMORY: {
                if (!mmu_) return RESULT_INVALID_HANDLE;
                uint64_t pa = 0;
                if (!mmu_->translate(args.x[1], args.x[2], false, false, pa))
                    return RESULT_INVALID_HANDLE;
                const emu::MemRegion* r = mmu_->find(args.x[1]);
                if (!r) return RESULT_INVALID_HANDLE;
                mmu_->map(args.x[0], pa, args.x[2], r->r, r->w, r->x);
                return RESULT_OK;
            }
            case SVC_UNMAP_MEMORY: {
                if (!mmu_) return RESULT_INVALID_HANDLE;
                const emu::MemRegion* r = mmu_->find(args.x[0]);
                if (!r || r->va_base != args.x[0] || r->size != args.x[1])
                    return RESULT_INVALID_HANDLE;
                mmu_->unmap(args.x[0]);
                return RESULT_OK;
            }
            case SVC_QUERY_MEMORY: {
                if (!mmu_ || !ram_) return RESULT_INVALID_HANDLE;
                MemInfo info;
                const emu::MemRegion* r = mmu_->find(args.x[2]);
                if (r) {
                    info.base = r->va_base;
                    info.size = r->size;
                    info.state = MEM_NORMAL;
                    info.perm = (r->r ? 1u : 0u) | (r->w ? 2u : 0u) | (r->x ? 4u : 0u);
                }
                uint64_t out = args.x[0];
                if (out + sizeof(MemInfo) > ram_size_) return RESULT_INVALID_HANDLE;
                uint8_t* d = ram_ + out;
                for (int i = 0; i < 8; i++) d[i] = static_cast<uint8_t>(info.base >> (8 * i));
                for (int i = 0; i < 8; i++) d[8 + i] = static_cast<uint8_t>(info.size >> (8 * i));
                for (int i = 0; i < 4; i++) d[16 + i] = static_cast<uint8_t>(info.state >> (8 * i));
                for (int i = 0; i < 4; i++) d[20 + i] = static_cast<uint8_t>(info.perm >> (8 * i));
                args.out[0] = 0;
                return RESULT_OK;
            }
            case SVC_CREATE_THREAD: {
                uint64_t id = sched_.spawn(args.x[1], args.x[2], 0, mmu_);
                args.out[0] = id;
                return RESULT_OK;
            }
            case SVC_SLEEP_THREAD: {
                slept_ns_ += args.x[0];
                return RESULT_OK;
            }
            case SVC_EXIT_PROCESS: {
                exited_ = true;
                return RESULT_OK;
            }
            case SVC_GET_INFO: {
                args.out[0] = 0;
                args.out[1] = 0;
                (void)args.x[1];
                return RESULT_OK;
            }
            // ===== IPC SVCs (expondo serviços para o guest) =====
            case 0x0E: // SVC_CREATE_PORT
                return svcCreatePort(args);
            case 0x0F: // SVC_MANAGE_NAMED_PORT
                return svcManageNamedPort(args);
            case 0x10: // SVC_CONNECT_TO_PORT
                return svcConnectToPort(args);
            case 0x11: // SVC_SEND_SYNC_REQUEST
                return svcSendSyncRequest(args);
            case 0x12: // SVC_REPLY_AND_RECEIVE
                return svcReplyAndReceive(args);
            case 0x13: // SVC_CLOSE_HANDLE
                return svcCloseHandle(args);
            case 0x14: // SVC_GET_THREAD_CONTEXT
                return svcGetThreadContext(args);
            case 0x15: // SVC_SET_THREAD_CONTEXT
                return svcSetThreadContext(args);
            case 0x16: // SVC_CREATE_MUTEX
                return svcCreateMutex(args);
            case 0x17: // SVC_LOCK_MUTEX
                return svcLockMutex(args);
            case 0x18: // SVC_UNLOCK_MUTEX
                return svcUnlockMutex(args);
            case 0x19: // SVC_CLOSE_MUTEX
                return svcCloseMutex(args);
            case 0x1A: // SVC_CREATE_EVENT
                return svcCreateEvent(args);
            case 0x1B: // SVC_SIGNAL_EVENT
                return svcSignalEvent(args);
            case 0x1C: // SVC_WAIT_EVENT
                return svcWaitEvent(args);
            case 0x1D: // SVC_CLEAR_EVENT
                return svcClearEvent(args);
            case 0x1E: // SVC_CLOSE_EVENT
                return svcCloseEvent(args);
            default:
                return RESULT_UNIMPLEMENTED;
        }
    }

private:
    SvcResult svcCreatePort(SvcArgs& args) {
        // X0 = name ptr (guest), X1 = max sessions, X2 = reply handle out, X3 = server handle out
        // Simplificado: cria porta anônima (server handle) e reply handle
        std::string name;
        uint64_t name_ptr = args.x[0];
        if (name_ptr && ram_) {
            for (size_t i = 0; i < 32 && name_ptr + i < ram_size_; i++) {
                char c = static_cast<char>(ram_[name_ptr + i]);
                if (c == 0) break;
                name += c;
            }
        }
        uint32_t server_h = services_.createPort(name, static_cast<uint32_t>(args.x[1]));
        uint32_t reply_h = createHandle(0x2000 | server_h); // client-side
        args.out[0] = reply_h;
        args.out[1] = server_h;
        return RESULT_OK;
    }

    SvcResult svcManageNamedPort(SvcArgs& args) {
        // X0 = operation (0=register, 1=unregister), X1 = name ptr, X2 = server handle
        // Stub: apenas ok
        (void)args.x[0]; (void)args.x[1]; (void)args.x[2];
        return RESULT_OK;
    }

    SvcResult svcConnectToPort(SvcArgs& args) {
        // X0 = name ptr, X1 = client handle out
        std::string name;
        uint64_t name_ptr = args.x[0];
        if (name_ptr && ram_) {
            for (size_t i = 0; i < 32 && name_ptr + i < ram_size_; i++) {
                char c = static_cast<char>(ram_[name_ptr + i]);
                if (c == 0) break;
                name += c;
            }
        }
        uint32_t server_h = services_.findPort(name);
        if (server_h == 0) return RESULT_INVALID_HANDLE;
        uint32_t client_h = createHandle(0x1000 | server_h);
        args.out[0] = client_h;
        return RESULT_OK;
    }

    SvcResult svcSendSyncRequest(SvcArgs& args) {
        // X0 = handle, X1..X7 = message + buffer descriptors
        // args.x[1] = message pointer (guest), args.x[2..7] = buffer descriptors
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag;
        if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        
        if (!ram_ || args.x[1] == 0) return RESULT_INVALID_HANDLE;
        
        // Parse message from guest memory
        uint64_t msg_ptr = args.x[1];
        if (msg_ptr + 32 > ram_size_) return RESULT_INVALID_HANDLE;
        
        IpcMessage req;
        uint8_t* msg_base = ram_ + msg_ptr;
        req.cmd = *reinterpret_cast<uint32_t*>(msg_base);
        uint32_t payload_size = *reinterpret_cast<uint32_t*>(msg_base + 4);
        uint32_t num_buffers = *reinterpret_cast<uint32_t*>(msg_base + 8);
        
        if (payload_size > 0 && msg_ptr + 12 + payload_size <= ram_size_) {
            req.payload.assign(msg_base + 12, msg_base + 12 + payload_size);
        }
        
        // Parse buffer descriptors (X2..X7)
        for (int i = 0; i < 6 && num_buffers > 0; i++) {
            uint64_t buf_desc_ptr = args.x[2 + i];
            if (buf_desc_ptr == 0 || buf_desc_ptr + 24 > ram_size_) break;
            uint8_t* buf_desc = ram_ + buf_desc_ptr;
            IpcMessage::Buffer buf;
            buf.guest_ptr = *reinterpret_cast<uint64_t*>(buf_desc);
            buf.size = *reinterpret_cast<uint64_t*>(buf_desc + 8);
            buf.kind = *reinterpret_cast<uint32_t*>(buf_desc + 16);
            buf.flags = *reinterpret_cast<uint32_t*>(buf_desc + 20);
            // Map host pointer for direct access
            if (buf.guest_ptr != 0 && buf.size != 0 && buf.guest_ptr + buf.size <= ram_size_) {
                buf.host_ptr = ram_ + buf.guest_ptr;
            }
            req.buffers.push_back(buf);
            num_buffers--;
        }
        
        // Encontra sessão pelo tag
        for (const auto& kv : services_.allSessions()) {
            if (kv.second->sendRequest(req)) {
                // Stub: ecoa de volta com mesmo cmd
                IpcMessage rep;
                rep.cmd = req.cmd;
                rep.payload = {1}; // success
                kv.second->sendReply(rep);
                break;
            }
        }
        return RESULT_OK;
    }

    SvcResult svcReplyAndReceive(SvcArgs& args) {
        // X0 = handles array ptr, X1 = count, X2 = timeout
        // X3 = reply message ptr, X4..X9 = reply buffer descriptors
        // Stub: processa uma resposta pendente e devolve o próximo pedido
        (void)args.x[0]; (void)args.x[1]; (void)args.x[2];
        
        // Se há uma resposta para enviar (X3), processa
        if (ram_ && args.x[3] != 0 && args.x[3] + 12 <= ram_size_) {
            uint64_t reply_ptr = args.x[3];
            uint8_t* reply_base = ram_ + reply_ptr;
            uint32_t reply_cmd = *reinterpret_cast<uint32_t*>(reply_base);
            uint32_t reply_payload_size = *reinterpret_cast<uint32_t*>(reply_base + 4);
            uint32_t reply_num_buffers = *reinterpret_cast<uint32_t*>(reply_base + 8);
            
            IpcMessage rep;
            rep.cmd = reply_cmd;
            if (reply_payload_size > 0 && reply_ptr + 12 + reply_payload_size <= ram_size_) {
                rep.payload.assign(reply_base + 12, reply_base + 12 + reply_payload_size);
            }
            
            // Parse reply buffer descriptors
            for (int i = 0; i < 6 && reply_num_buffers > 0; i++) {
                uint64_t buf_desc_ptr = args.x[4 + i];
                if (buf_desc_ptr == 0 || buf_desc_ptr + 24 > ram_size_) break;
                uint8_t* buf_desc = ram_ + buf_desc_ptr;
                IpcMessage::Buffer buf;
                buf.guest_ptr = *reinterpret_cast<uint64_t*>(buf_desc);
                buf.size = *reinterpret_cast<uint64_t*>(buf_desc + 8);
                buf.kind = *reinterpret_cast<uint32_t*>(buf_desc + 16);
                buf.flags = *reinterpret_cast<uint32_t*>(buf_desc + 20);
                if (buf.guest_ptr != 0 && buf.size != 0 && buf.guest_ptr + buf.size <= ram_size_) {
                    buf.host_ptr = ram_ + buf.guest_ptr;
                }
                rep.buffers.push_back(buf);
                reply_num_buffers--;
            }
            
            // Envia a resposta para a sessão correspondente
            // (simplificado: envia para primeira sessão disponível)
            for (const auto& kv : services_.allSessions()) {
                if (kv.second->sendReply(rep)) break;
            }
        }
        
        // Agora espera o próximo pedido (simplificado: retorna vazio)
        // Em implementação real, bloquearia a thread até chegar pedido
        args.out[0] = 0; // handle do próximo pedido
        args.out[1] = 0;
        return RESULT_OK;
    }

    SvcResult svcCloseHandle(SvcArgs& args) {
        // X0 = handle
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        if (!closeHandle(h)) return RESULT_INVALID_HANDLE;
        return RESULT_OK;
    }

    SvcResult svcGetThreadContext(SvcArgs& args) {
        // X0 = thread handle, X1 = context ptr out
        // Stub: zeros
        if (!ram_) return RESULT_INVALID_HANDLE;
        uint64_t out = args.x[1];
        if (out + 256 > ram_size_) return RESULT_INVALID_HANDLE;
        for (size_t i = 0; i < 256; i++) ram_[out + i] = 0;
        return RESULT_OK;
    }

    SvcResult svcSetThreadContext(SvcArgs& args) {
        // X0 = thread handle, X1 = context ptr
        // Stub: ok
        (void)args.x[0]; (void)args.x[1];
        return RESULT_OK;
    }

    // ===== Mutex SVCs =====
    SvcResult svcCreateMutex(SvcArgs& args) {
        // X0 = mutex handle out
        uint32_t id = mutexes_.create();
        args.out[0] = createHandle(0x3000 | id);
        return RESULT_OK;
    }
    SvcResult svcLockMutex(SvcArgs& args) {
        // X0 = mutex handle, X1 = timeout (ignored), X2 = owner thread id
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t mid = tag & 0xFFF;
        uint64_t owner = args.x[2] ? args.x[2] : sched_.currentThreadId();
        if (!mutexes_.lock(mid, owner)) return RESULT_INVALID_HANDLE;
        return RESULT_OK;
    }
    SvcResult svcUnlockMutex(SvcArgs& args) {
        // X0 = mutex handle, X1 = owner thread id
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t mid = tag & 0xFFF;
        uint64_t owner = args.x[1] ? args.x[1] : sched_.currentThreadId();
        if (!mutexes_.unlock(mid, owner)) return RESULT_INVALID_HANDLE;
        return RESULT_OK;
    }
    SvcResult svcCloseMutex(SvcArgs& args) {
        // X0 = mutex handle
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t mid = tag & 0xFFF;
        if (!mutexes_.close(mid)) return RESULT_INVALID_HANDLE;
        closeHandle(h);
        return RESULT_OK;
    }

    // ===== Event SVCs =====
    SvcResult svcCreateEvent(SvcArgs& args) {
        // X0 = event handle out, X1 = reset type (0=auto, 1=manual)
        uint32_t id = events_.create(static_cast<uint32_t>(args.x[1]));
        args.out[0] = createHandle(0x4000 | id);
        return RESULT_OK;
    }
    SvcResult svcSignalEvent(SvcArgs& args) {
        // X0 = event handle
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t eid = tag & 0xFFF;
        events_.signal(eid);
        return RESULT_OK;
    }
    SvcResult svcWaitEvent(SvcArgs& args) {
        // X0 = event handle, X1 = timeout ns (0 = infinite)
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t eid = tag & 0xFFF;
        uint64_t timeout = args.x[1];
        events_.wait(eid, timeout);
        return RESULT_OK;
    }
    SvcResult svcClearEvent(SvcArgs& args) {
        // X0 = event handle
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t eid = tag & 0xFFF;
        events_.clear(eid);
        return RESULT_OK;
    }
    SvcResult svcCloseEvent(SvcArgs& args) {
        // X0 = event handle
        uint32_t h = static_cast<uint32_t>(args.x[0]);
        uint32_t tag; if (!getHandle(h, tag)) return RESULT_INVALID_HANDLE;
        uint32_t eid = tag & 0xFFF;
        events_.close(eid);
        closeHandle(h);
        return RESULT_OK;
    }

private:
    uint64_t heap_base_ = 0x08000000; // base típica do heap do app
    uint64_t heap_size_ = 0;
    uint64_t last_mem_attr_ = 0;
    bool exited_ = false;
    uint64_t slept_ns_ = 0;
    uint64_t next_pid_ = 1;
    std::unordered_map<uint64_t, Process> processes_;
    uint32_t next_handle_ = 1;
    std::unordered_map<uint32_t, uint32_t> handles_;
    emu::Mmu* mmu_ = nullptr;
    uint8_t* ram_ = nullptr;
    uint64_t ram_size_ = 0;
    Scheduler sched_;
    ServiceManager services_;
    MutexTable mutexes_;
    EventTable events_;
    NvService nv_;
    ViService vi_;
    AudService aud_;
    FsService fs_;
    HidService hid_;
    TimeService time_;
    ApmService apm_;
    AccService acc_;
    AppletService applet_;
    PsmService psm_;
    LblService lbl_;
    SetService set_;
    FatalService fatal_;
    PmService pm_;
};

} // namespace hos
} // namespace mgd
