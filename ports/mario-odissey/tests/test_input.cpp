#include <cassert>
#include <cstdio>
#include "../src/runtime/Input.h"

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

    std::printf("input tests passed!\n");
    return 0;
}
