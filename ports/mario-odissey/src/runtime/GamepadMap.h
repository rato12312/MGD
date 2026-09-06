#pragma once

#include "Input.h"
#include <cstdint>

namespace port {

// Mapeamento de controles externos (ex.: GameSir X5 Lite) para o núcleo.
// Gamepads Android/XInput chegam como botões padrão + eixos; aqui vira
// InputState do port. Sem dependência de SO: o frontend preenche e chama.
struct StdGamepad {
    // Botões no padrão comum (A/B/X/Y, LB/RB, LT/RT, Start/Select, D-pad, sticks click)
    uint32_t buttons = 0;
    float lx = 0.0f, ly = 0.0f; // analógico esquerdo -1..1
    float rx = 0.0f, ry = 0.0f; // analógico direito -1..1

    static constexpr uint32_t GP_A = 1u << 0;
    static constexpr uint32_t GP_B = 1u << 1;
    static constexpr uint32_t GP_X = 1u << 2;
    static constexpr uint32_t GP_Y = 1u << 3;
    static constexpr uint32_t GP_LB = 1u << 4;
    static constexpr uint32_t GP_RB = 1u << 5;
    static constexpr uint32_t GP_LT = 1u << 6;
    static constexpr uint32_t GP_RT = 1u << 7;
    static constexpr uint32_t GP_START = 1u << 8;
    static constexpr uint32_t GP_SELECT = 1u << 9;
    static constexpr uint32_t GP_UP = 1u << 10;
    static constexpr uint32_t GP_DOWN = 1u << 11;
    static constexpr uint32_t GP_LEFT = 1u << 12;
    static constexpr uint32_t GP_RIGHT = 1u << 13;
};

// Converte gamepad padrão -> InputState do Switch (layout Odyssey).
// Nota: Switch B é "confirmar" (em baixo) e A é "voltar" (direita),
// então mapeamos GP_A->B e GP_B->A para o dedo não estranhar.
inline void mapGamepad(const StdGamepad& gp, InputState& out) {
    out.clearButtons();
    if (gp.buttons & StdGamepad::GP_A) out.press(Buttons::B);
    if (gp.buttons & StdGamepad::GP_B) out.press(Buttons::A);
    if (gp.buttons & StdGamepad::GP_X) out.press(Buttons::Y);
    if (gp.buttons & StdGamepad::GP_Y) out.press(Buttons::X);
    if (gp.buttons & StdGamepad::GP_LB) out.press(Buttons::L);
    if (gp.buttons & StdGamepad::GP_RB) out.press(Buttons::R);
    if (gp.buttons & StdGamepad::GP_LT) out.press(Buttons::ZL);
    if (gp.buttons & StdGamepad::GP_RT) out.press(Buttons::ZR);
    if (gp.buttons & StdGamepad::GP_START) out.press(Buttons::PLUS);
    if (gp.buttons & StdGamepad::GP_SELECT) out.press(Buttons::MINUS);
    if (gp.buttons & StdGamepad::GP_UP) out.press(Buttons::UP);
    if (gp.buttons & StdGamepad::GP_DOWN) out.press(Buttons::DOWN);
    if (gp.buttons & StdGamepad::GP_LEFT) out.press(Buttons::LEFT);
    if (gp.buttons & StdGamepad::GP_RIGHT) out.press(Buttons::RIGHT);
    out.setLeftStick(gp.lx, gp.ly);
    out.right_stick.x = gp.rx;
    out.right_stick.y = gp.ry;
}

} // namespace port
