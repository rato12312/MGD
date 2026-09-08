// mgd_nsp: inspeciona NSP (PFS0) com os loaders do emulador.
// Uso: mgd_nsp jogo.nsp
// Só lê e lista (nomes + tamanhos) + sonda NCAs. Não extrai, não descriptografa.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "emulador-mgd/loader/Pfs0.h"
#include "emulador-mgd/loader/NcaProbe.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("uso: mgd_nsp jogo.nsp\n");
        return 1;
    }
    std::ifstream f(argv[1], std::ios::binary | std::ios::ate);
    if (!f) {
        std::printf("nao abriu %s\n", argv[1]);
        return 1;
    }
    size_t len = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> blob(len);
    f.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(len));
    mgd::emu::Pfs0Reader pfs;
    if (!pfs.open(blob.data(), blob.size())) {
        std::printf("nao e PFS0\n");
        return 1;
    }
    std::printf("%zu arquivos:\n", pfs.fileCount());
    for (size_t i = 0; i < pfs.fileCount(); i++) {
        std::vector<uint8_t> data;
        std::string name = pfs.fileName(i);
        if (!pfs.readFile(name, data)) {
            std::printf("  %s: ERRO leitura\n", name.c_str());
            continue;
        }
        std::string extra;
        mgd::emu::NcaProbe probe = mgd::emu::probeNca(data.data(), data.size());
        if (probe.valid) {
            char tag[64];
            std::snprintf(tag, sizeof(tag), " [NCA %.4s tipo=%u]", probe.magic, probe.content_type);
            extra = tag;
        }
        std::printf("  %s (%zu bytes)%s\n", name.c_str(), data.size(), extra.c_str());
    }
    return 0;
}
