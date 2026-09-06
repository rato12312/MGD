#include <cassert>
#include <cstdio>
#include "../src/runtime/Input.h"
#include "../src/runtime/GamepadMap.h"

int main() {
    port::InputState in;
    assert(!in.down(port::Buttons::A));
    in.press(port::Buttons::A);
    in.press(port::Buttons::PLUS);
    assert(in.down(port::Buttons::A));
    assert(in.down(port::Buttons::PLUS));
    assert(!in.down(port::Buttons::B));
    in.release(port::Buttons::A);
    assert(!in.down(port::Buttons::A));
    assert(in.down(port::Buttons::PLUS));
    in.clearButtons();
    assert(!in.down(port::Buttons::PLUS));

    in.setLeftStick(0.05f, -0.05f); // dentro da deadzone
    assert(in.left_stick.x == 0.0f && in.left_stick.y == 0.0f);
    in.setLeftStick(0.5f, 0.0f);
    assert(in.left_stick.x == 0.5f);
    assert(in.left_stick.magnitude() == 0.25f); // clampado em 1.0 (0.25)
    in.setLeftStick(1.0f, 1.0f);
    assert(in.left_stick.magnitude() == 1.0f); // clamp

    // GameSir X5 Lite (padrão): A<->B trocados pro layout do Switch
    port::StdGamepad gp;
    gp.buttons = port::StdGamepad::GP_A | port::StdGamepad::GP_START;
    gp.lx = 0.0f; gp.ly = -1.0f;
    port::InputState mapped;
    port::mapGamepad(gp, mapped);
    assert(mapped.down(port::Buttons::B)); // A do controle = B do Switch
    assert(!mapped.down(port::Buttons::A));
    assert(mapped.down(port::Buttons::PLUS));
    assert(mapped.left_stick.y == -1.0f);

    std::printf("input tests passed!\n");
    return 0;
}
