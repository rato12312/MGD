#pragma once

#include <cstdint>

namespace port {

// Núcleo de input do port (independente de Android/toque).
// O frontend (toque, controle) só preenche este estado; o jogo lê daqui.
struct Buttons {
    static constexpr uint32_t A = 1u << 0;
    static constexpr uint32_t B = 1u << 1;
    static constexpr uint32_t X = 1u << 2;
    static constexpr uint32_t Y = 1u << 3;
    static constexpr uint32_t L = 1u << 4;
    static constexpr uint32_t R = 1u << 5;
    static constexpr uint32_t ZL = 1u << 6;
    static constexpr uint32_t ZR = 1u << 7;
    static constexpr uint32_t PLUS = 1u << 8;
    static constexpr uint32_t MINUS = 1u << 9;
    static constexpr uint32_t UP = 1u << 10;
    static constexpr uint32_t DOWN = 1u << 11;
    static constexpr uint32_t LEFT = 1u << 12;
    static constexpr uint32_t RIGHT = 1u << 13;
};

struct AnalogStick {
    // -1.0 .. +1.0, com deadzone aplicada na leitura
    float x = 0.0f;
    float y = 0.0f;

    float magnitude() const {
        float m = x * x + y * y;
        return m > 1.0f ? 1.0f : m; // magnitude² clampada (barata, sem sqrt)
    }
};

struct InputState {
    uint32_t buttons = 0;
    AnalogStick left_stick;
    AnalogStick right_stick;

    void press(uint32_t b) { buttons |= b; }
    void release(uint32_t b) { buttons &= ~b; }
    bool down(uint32_t b) const { return (buttons & b) != 0; }
    void clearButtons() { buttons = 0; }

    void setLeftStick(float x, float y) {
        // deadzone simples: abaixo de 0.15 vira zero
        left_stick.x = (x > -0.15f && x < 0.15f) ? 0.0f : x;
        left_stick.y = (y > -0.15f && y < 0.15f) ? 0.0f : y;
    }
};

} // namespace port
