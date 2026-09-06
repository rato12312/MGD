#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace port {

// Custo de conversão entre formatos de textura, inspirado na tabela de
// formatos compatíveis do Eden — transformado para decisão MGD:
// em vez de só dizer "compatível ou não", diz quanto custa.
// Na Mali, view mutável é cara (quirk do log): pares mutáveis têm custo alto
// e o port prefere cachear a view pronta (ViewCache) a reconverter.
enum class FormatCost : uint8_t {
    Zero = 0,   // mesmo formato: só reutiliza a view
    Low = 1,    // swizzle/reinterpretação barata
    High = 2,   // mutável na Mali: cacheia, nunca reconverte por frame
    Blocked = 3 // sem caminho: usa fallback (ex.: formato próximo + aviso)
};

struct FormatPair {
    std::string from;
    std::string to;
    FormatCost cost = FormatCost::Zero;
};

class FormatCostTable {
public:
    FormatCostTable() {
        // Pares vistos no log do Odyssey (sRGB <-> Unorm o tempo todo).
        add("R8G8B8A8Unorm", "R8G8B8A8Unorm", FormatCost::Zero);
        add("R8G8B8A8Srgb", "R8G8B8A8Srgb", FormatCost::Zero);
        add("R8G8B8A8Unorm", "R8G8B8A8Srgb", FormatCost::High);
        add("R8G8B8A8Srgb", "R8G8B8A8Unorm", FormatCost::High);
        add("B8G8R8A8Unorm", "R8G8B8A8Unorm", FormatCost::Low);
        add("R8G8B8A8Srgb", "B8G8R8A8Srgb", FormatCost::Low);
    }

    FormatCost cost(const std::string& from, const std::string& to) const {
        for (const auto& p : pairs_) {
            if (p.from == from && p.to == to) return p.cost;
        }
        return FormatCost::Blocked; // desconhecido = não inventa, usa fallback
    }

    // true se vale a pena cachear a view pronta em vez de reconverter.
    bool shouldCache(const std::string& from, const std::string& to) const {
        return cost(from, to) != FormatCost::Zero;
    }

    void add(const std::string& from, const std::string& to, FormatCost c) {
        pairs_.push_back(FormatPair{from, to, c});
    }

    size_t size() const { return pairs_.size(); }

private:
    std::vector<FormatPair> pairs_;
};

} // namespace port
