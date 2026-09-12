#pragma once

// Sessão IPC mínima: par cliente/servidor com filas nos dois sentidos.
// Sem número SVC ainda — API direta; o SVC entra quando os números
// estiverem confirmados. Falha fechada em tudo.

#include <cstdint>
#include <deque>
#include <vector>
#include <utility>

#include "Hipc.h"

namespace mgd {
namespace hos {

struct IpcMessage {
    uint32_t cmd = 0;
    std::vector<uint8_t> payload;
    // Buffers anexados (X/A/B/W): viajam junto, sem copiar o conteúdo.
    struct Buffer {
        uint64_t guest_ptr = 0;
        uint64_t size = 0;
        uint32_t kind = 0; // 0=X(envia) 1=A(recebe) 2=B(mapa) 3=W(escrita)
        uint32_t flags = 0; // Buffer flags
        // Host-side mapping (preenchido pelo kernel no send/receive)
        uint8_t* host_ptr = nullptr;
    };
    std::vector<Buffer> buffers;
};

class Session {
public:
    // Lado cliente: envia pedido, lê resposta.
    bool sendRequest(const IpcMessage& m) {
        if (to_server_.size() >= kMaxQueue) return false;
        to_server_.push_back(m);
        return true;
    }
    bool recvReply(IpcMessage& out) {
        if (to_client_.empty()) return false;
        out = to_client_.front();
        to_client_.pop_front();
        return true;
    }
    // Lado servidor: lê pedido, envia resposta.
    bool recvRequest(IpcMessage& out) {
        if (to_server_.empty()) return false;
        out = to_server_.front();
        to_server_.pop_front();
        return true;
    }
    bool sendReply(const IpcMessage& m) {
        if (to_client_.size() >= kMaxQueue) return false;
        to_client_.push_back(m);
        return true;
    }
    size_t pendingRequests() const { return to_server_.size(); }

    // Buffer translation helpers (chamado pelo kernel)
    static void translateBuffers(const IpcMessage& msg, uint8_t* ram, uint64_t ram_size,
                                 std::vector<std::pair<uint64_t, uint8_t*>>& out_mappings) {
        for (const auto& buf : msg.buffers) {
            if (buf.guest_ptr == 0 || buf.size == 0) continue;
            if (buf.guest_ptr + buf.size > ram_size) continue;
            out_mappings.emplace_back(buf.guest_ptr, ram + buf.guest_ptr);
        }
    }

    // Convert between Session buffers and HIPC buffers
    static HipcBufferDesc toHipcBuffer(const Buffer& b) {
        HipcBufferDesc h;
        h.type = static_cast<HipcBufferType>(b.kind);
        h.flags = b.flags;
        h.addr = b.guest_ptr;
        h.size = b.size;
        return h;
    }
    static Buffer fromHipcBuffer(const HipcBufferDesc& h) {
        Buffer b;
        b.guest_ptr = h.addr;
        b.size = h.size;
        b.kind = static_cast<uint32_t>(h.type);
        b.flags = h.flags;
        return b;
    }

private:
    static constexpr size_t kMaxQueue = 64;
    std::deque<IpcMessage> to_server_;
    std::deque<IpcMessage> to_client_;
};

} // namespace hos
} // namespace mgd
