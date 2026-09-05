#pragma once

#include "../../common/Vec3.h"
#include <cstddef>

namespace mgd {
namespace seed {

// Preditor de câmera: extrapola a próxima pose (posição + direção) a partir
// do histórico. O shader pré-resolve o que estará visível lá, então o frame
// seguinte vira reutilização. Base para sombreamento especulativo.
struct CameraPose {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, 1.0f};
};

class CameraPredictor {
public:
    // Informa a pose atual deste frame.
    void observe(const Vec3& position, const Vec3& forward) {
        prev2_ = prev1_;
        prev1_ = CameraPose{position, forward};
        have2_ = have1_;
        have1_ = true;
    }

    // Prevê a próxima pose por extrapolação linear do movimento.
    // Sem histórico suficiente, repete a última pose (sem inventar movimento).
    CameraPose predictNext() const {
        if (!have1_) return CameraPose{};
        if (!have2_) return prev1_;
        Vec3 dp = prev1_.position - prev2_.position;
        Vec3 df = prev1_.forward - prev2_.forward;
        CameraPose out;
        out.position = prev1_.position + dp;
        out.forward = prev1_.forward + df;
        float len = out.forward.length();
        if (len > 1e-6f) out.forward = out.forward / len;
        else out.forward = prev1_.forward;
        return out;
    }

    // Quão confiável é a previsão (1 = parado, cai com movimento brusco).
    // O MGD só pré-resolve quando a confiança está alta.
    float confidence() const {
        if (!have2_) return 1.0f;
        Vec3 dp = prev1_.position - prev2_.position;
        Vec3 df = prev1_.forward - prev2_.forward;
        float move = dp.length() + df.length() * 10.0f;
        float c = 1.0f - move * 0.05f;
        if (c < 0.0f) c = 0.0f;
        return c;
    }

    void reset() { have1_ = have2_ = false; }

private:
    CameraPose prev1_, prev2_;
    bool have1_ = false, have2_ = false;
};

} // namespace seed
} // namespace mgd
