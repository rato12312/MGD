// Lista seções de um NCA (só cabeçalho, sem descriptografar).
// Uso: nca_list <arquivo.nca>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include "../src/loader/NcaHeader.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::printf("uso: nca_list <arquivo.nca>\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::printf("erro: nao abriu %s\n", argv[1]);
        return 1;
    }
    std::vector<uint8_t> bytes(port::NcaHeader::SIZE);
    in.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (static_cast<size_t>(in.gcount()) < port::NcaHeader::SIZE) {
        std::printf("erro: arquivo menor que cabecalho NCA\n");
        return 1;
    }
    auto h = port::NcaHeader::parse(bytes);
    if (!h) {
        std::printf("erro: magic NCA3 ausente (arquivo criptografado demais para ler?)\n");
        return 1;
    }
    std::printf("NCA ok: distribution=%u content_type=%u\n", h->distribution, h->content_type);
    for (int i = 0; i < 4; ++i) {
        std::printf("  secao %d: media [%u, %u)\n", i,
                    h->sections[i].media_offset, h->sections[i].media_end_offset);
    }
    std::printf("nota: conteudo criptografado (AES-XTS) — descriptografia exige keys do console\n");
    return 0;
}
