#pragma once

// Kernel HOS mínimo: tabela SVC + resultados.
// Cada chamada real cresce aqui. Desconhecida = erro, nunca trava.

#include <cstdint>

namespace mgd {
namespace hos {

// Números SVC do Horizon OS (subset inicial).
enum SvcNumber : uint32_t {
    SVC_SET_HEAP_SIZE = 0x01,
    SVC_EXIT_PROCESS = 0x06,
    SVC_GET_INFO = 0x29,
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

class Kernel {
public:
    Kernel() = default;

    uint64_t heapBase() const { return heap_base_; }
    uint64_t heapSize() const { return heap_size_; }
    bool exited() const { return exited_; }

    SvcResult call(uint32_t num, SvcArgs& args) {
        switch (num) {
            case SVC_SET_HEAP_SIZE: {
                // X1 = tamanho pedido; devolve base em out[0].
                heap_size_ = args.x[1];
                args.out[0] = heap_base_;
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
};

} // namespace hos
} // namespace mgd
