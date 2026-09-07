#pragma once

// Kernel HOS mínimo: tabela SVC + resultados.
// Cada chamada real cresce aqui. Desconhecida = erro, nunca trava.

#include <cstdint>

#include <unordered_map>

#include "../ram/Mmu.h"
#include "AudService.h"
#include "FsService.h"
#include "HidService.h"
#include "NvService.h"
#include "ViService.h"
#include "ServiceManager.h"
#include "Thread.h"

namespace mgd {
namespace hos {

// Números SVC do Horizon OS (subset inicial).
enum SvcNumber : uint32_t {
    SVC_SET_HEAP_SIZE = 0x01,
    SVC_SET_MEMORY_PERMISSION = 0x02, // X0=addr X1=size X2=perm(rwx bits)
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

class Kernel : public emu::SvcHost {
public:
    Kernel() = default;

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
            if (understood && kv.second->sendReply(rep)) done++;
        }
        return done;
    }
    NvService& nv() { return nv_; }
    ViService& vi() { return vi_; }
    AudService& aud() { return aud_; }
    FsService& fs() { return fs_; }
    HidService& hid() { return hid_; }

    SvcResult call(uint32_t num, SvcArgs& args) {
        switch (num) {
            case SVC_SET_HEAP_SIZE: {
                // X1 = tamanho pedido; devolve base em out[0] e mapeia o heap.
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
            case SVC_MAP_MEMORY: {
                // Alias: dst enxerga o físico de src (mesma permissão).
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
                // X0 = out (MemInfo no guest), X2 = endereço consultado.
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
                uint64_t id = sched_.spawn(args.x[1], args.x[2]);
                args.out[0] = id;
                return RESULT_OK;
            }
            case SVC_SLEEP_THREAD: {
                // Single-thread: não há quem acordar; só volta OK.
                slept_ns_ += args.x[0];
                return RESULT_OK;
            }
            case SVC_EXIT_PROCESS: {
                exited_ = true;
                return RESULT_OK;
            }
            case SVC_GET_INFO: {
                // X1 = id; devolve 0 (stub honesto até mapear infos reais).
                args.out[0] = 0;
                args.out[1] = 0;
                (void)args.x[1];
                return RESULT_OK;
            }
            default:
                return RESULT_UNIMPLEMENTED;
        }
    }

private:
    uint64_t heap_base_ = 0x08000000; // base típica do heap do app
    uint64_t heap_size_ = 0;
    bool exited_ = false;
    uint64_t slept_ns_ = 0;
    uint32_t next_handle_ = 1;
    std::unordered_map<uint32_t, uint32_t> handles_;
    emu::Mmu* mmu_ = nullptr;
    Scheduler sched_;
    ServiceManager services_;
    NvService nv_;
    ViService vi_;
    AudService aud_;
    FsService fs_;
    HidService hid_;
};

} // namespace hos
} // namespace mgd
