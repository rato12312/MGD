#pragma once

// appletOE (applet manager) — fluxo real de boot do app.
// cmd 1  = GetAppletResourceUserId -> user id
// cmd 2  = CreateApplet -> aloca applet, responde handle
// cmd 3  = StartApplet -> inicia (carrega main.nso, roda entry)
// cmd 4  = PushApplet -> empilha applet sobre o atual
// cmd 5  = PopApplet -> remove topo
// cmd 6  = GetAppletState -> running/suspended/exited
// cmd 7  = NotifyRunning -> avisa que applet está rodando
// cmd 8  = GetSharedFontSharedMemoryHandle -> handle de fonte
// cmd 9  = GetSharedFontInlines -> dados da fonte
// cmd 10 = GetLibraryAppletCreator -> para libapplets (web, kb, etc.)
// cmd 11 = GetAppletOperationMode -> operation mode
// cmd 12 = RegisterAppletResourceUserId -> register user id

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "Session.h"
#include "../cpu/Cpu.h"
#include "../loader/NsoLoader.h"

namespace mgd {
namespace hos {

enum class AppletState : uint8_t {
    CREATED = 0,
    RUNNING = 1,
    SUSPENDED = 2,
    EXITED = 3,
};

struct Applet {
    uint64_t id = 0;
    std::string name;
    uint64_t program_id = 0;
    uint64_t entry_point = 0;
    uint64_t stack_top = 0;
    AppletState state = AppletState::CREATED;
    std::vector<uint8_t> nso_blob; // main.nso carregado
    
    // Boot callback (set by emulator)
    std::function<bool(uint64_t)> on_boot;
};

class AppletService {
public:
    AppletService() = default;
    
    // Set boot callback (called when applet NSO should be booted)
    void setBootCallback(std::function<bool(uint64_t)> cb) { boot_cb_ = std::move(cb); }

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) { // GetAppletResourceUserId
            rep.cmd = 1;
            rep.payload = {1, 0, 0, 0, 0, 0, 0, 0}; // self = 1
            return true;
        }
        if (req.cmd == 2) { // CreateApplet
            // payload: name('\0') + program_id(8)
            size_t z = 0;
            while (z < req.payload.size() && req.payload[z] != 0) z++;
            if (z >= req.payload.size() || z + 9 > req.payload.size()) {
                rep.cmd = 0; return true;
            }
            std::string name(req.payload.begin(), req.payload.begin() + z);
            uint64_t pid = 0;
            for (int i = 0; i < 8; i++) pid |= static_cast<uint64_t>(req.payload[z + 1 + i]) << (8 * i);
            uint64_t aid = next_applet_++;
            applets_[aid] = Applet{aid, name, pid, 0, 0, AppletState::CREATED, {}};
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(aid & 0xFF),
                           static_cast<uint8_t>((aid >> 8) & 0xFF),
                           static_cast<uint8_t>((aid >> 16) & 0xFF),
                           static_cast<uint8_t>((aid >> 24) & 0xFF),
                           static_cast<uint8_t>((aid >> 32) & 0xFF),
                           static_cast<uint8_t>((aid >> 40) & 0xFF),
                           static_cast<uint8_t>((aid >> 48) & 0xFF),
                           static_cast<uint8_t>((aid >> 56) & 0xFF)};
            return true;
        }
        if (req.cmd == 3) { // StartApplet
            // payload: applet_id(8) + nso_blob
            if (req.payload.size() < 8) { rep.cmd = 0; return true; }
            uint64_t aid = rd64(req.payload, 0);
            auto it = applets_.find(aid);
            if (it == applets_.end()) { rep.cmd = 0; return true; }
            Applet& a = it->second;
            a.nso_blob.assign(req.payload.begin() + 8, req.payload.end());
            // Parse NSO entry point
            if (a.nso_blob.size() >= 0x100 && a.nso_blob[0]=='N'&&a.nso_blob[1]=='S'&&a.nso_blob[2]=='O'&&a.nso_blob[3]=='0') {
                uint32_t entry_off = rd32(a.nso_blob, 0x18);
                a.entry_point = entry_off;
            }
            a.state = AppletState::RUNNING;
            
            // Boot the applet NSO
            if (boot_cb_) {
                bool ok = boot_cb_(aid);
                if (!ok) {
                    a.state = AppletState::EXITED;
                    rep.cmd = 0;
                    return true;
                }
            }
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 4) { // PushApplet
            // payload: applet_id(8)
            if (req.payload.size() < 8) { rep.cmd = 0; return true; }
            uint64_t aid = rd64(req.payload, 0);
            if (applets_.find(aid) != applets_.end()) stack_.push_back(aid);
            rep.cmd = 1; return true;
        }
        if (req.cmd == 5) { // PopApplet
            if (!stack_.empty()) {
                uint64_t aid = stack_.back(); stack_.pop_back();
                auto it = applets_.find(aid);
                if (it != applets_.end()) it->second.state = AppletState::SUSPENDED;
            }
            rep.cmd = 1; return true;
        }
        if (req.cmd == 6) { // GetAppletState
            if (req.payload.size() < 8) { rep.cmd = 0; return true; }
            uint64_t aid = rd64(req.payload, 0);
            auto it = applets_.find(aid);
            uint8_t st = it != applets_.end() ? static_cast<uint8_t>(it->second.state) : 0xFF;
            rep.cmd = 1; rep.payload = {st}; return true;
        }
        if (req.cmd == 7) { // NotifyRunning
            if (req.payload.size() < 8) { rep.cmd = 0; return true; }
            uint64_t aid = rd64(req.payload, 0);
            auto it = applets_.find(aid);
            if (it != applets_.end()) it->second.state = AppletState::RUNNING;
            rep.cmd = 1; return true;
        }
        if (req.cmd == 8) { // GetSharedFontSharedMemoryHandle
            // Retorna handle fake para memória compartilhada de fonte
            rep.cmd = 1;
            rep.payload = {0x01, 0x00, 0x00, 0x00}; return true;
        }
        if (req.cmd == 9) { // GetSharedFontInlines
            // Dados da fonte (stub vazio)
            rep.cmd = 1; return true;
        }
        if (req.cmd == 10) { // GetLibraryAppletCreator
            rep.cmd = 1; return true;
        }
        if (req.cmd == 11) { // GetAppletOperationMode
            // 0 = normal, 1 = debug, etc.
            rep.cmd = 1;
            rep.payload = {0, 0, 0, 0, 0, 0, 0, 0}; return true;
        }
        if (req.cmd == 12) { // RegisterAppletResourceUserId
            // Registra user id para applet
            rep.cmd = 1;
            rep.payload = {0}; return true;
        }
        return false;
    }

    // Acesso para o Emulador
    const Applet* getApplet(uint64_t id) const {
        auto it = applets_.find(id);
        return it != applets_.end() ? &it->second : nullptr;
    }
    Applet* getApplet(uint64_t id) {
        auto it = applets_.find(id);
        return it != applets_.end() ? &it->second : nullptr;
    }
    std::vector<uint64_t> stack() const { return stack_; }

    // Boot applet NSO (called by emulator)
    bool bootApplet(uint64_t aid, Cpu& cpu, Mmu& mmu) {
        auto it = applets_.find(aid);
        if (it == applets_.end()) return false;
        Applet& a = it->second;
        
        if (a.nso_blob.empty()) return false;
        
        // Parse NSO
        NsoImage img = parseNso(a.nso_blob.data(), a.nso_blob.size());
        if (!img.valid) return false;
        
        uint64_t entry = 0;
        if (!loadNsoInto(img, a.nso_blob.data(), cpu.ram(), cpu.ramSize(), 0x10000000, entry)) return false;
        
        a.entry_point = entry;
        
        // Setup CPU for applet
        cpu.setSp(0x80000000); // applet stack
        cpu.setPc(entry + 0x80); // skip header
        
        return true;
    }

    // Get applet NSO for manual boot
    const std::vector<uint8_t>* getAppletNso(uint64_t aid) const {
        auto it = applets_.find(aid);
        if (it != applets_.end() && !it->second.nso_blob.empty()) {
            return &it->second.nso_blob;
        }
        return nullptr;
    }

    // Snapshot support
    struct AppletSnapshot {
        uint64_t id;
        std::string name;
        uint64_t program_id;
        uint64_t entry_point;
        uint64_t stack_top;
        uint8_t state;
        std::vector<uint8_t> nso_blob;
    };
    std::vector<Kernel::AppletSnapshot> getAppletsForSnapshot() const {
        std::vector<Kernel::AppletSnapshot> out;
        out.reserve(applets_.size());
        for (const auto& [id, a] : applets_) {
            out.push_back({a.id, a.name, a.program_id, a.entry_point, a.stack_top, 
                          static_cast<uint8_t>(a.state), a.nso_blob});
        }
        return out;
    }
    std::vector<uint64_t> stack() const { return stack_; }
    uint64_t getNextAppletId() const { return next_applet_; }
    
    void restoreApplet(const Kernel::AppletSnapshot& a) {
        Applet applet;
        applet.id = a.id;
        applet.name = a.name;
        applet.program_id = a.program_id;
        applet.entry_point = a.entry_point;
        applet.stack_top = a.stack_top;
        applet.state = static_cast<AppletState>(a.state);
        applet.nso_blob = a.nso_blob;
        applets_[a.id] = std::move(applet);
    }
    void restoreStack(const std::vector<uint64_t>& s) { stack_ = s; }
    void setNextAppletId(uint64_t v) { next_applet_ = v; }

private:
    static uint32_t rd32(const std::vector<uint8_t>& v, size_t o) {
        uint32_t r = 0;
        for (int i = 0; i < 4; i++) r |= static_cast<uint32_t>(v[o + i]) << (8 * i);
        return r;
    }
    static uint64_t rd64(const std::vector<uint8_t>& v, size_t o) {
        uint64_t r = 0;
        for (int i = 0; i < 8; i++) r |= static_cast<uint64_t>(v[o + i]) << (8 * i);
        return r;
    }

    std::unordered_map<uint64_t, Applet> applets_;
    std::vector<uint64_t> stack_;
    uint64_t next_applet_ = 1;
    std::function<bool(uint64_t)> boot_cb_;
};

} // namespace hos
} // namespace mgd