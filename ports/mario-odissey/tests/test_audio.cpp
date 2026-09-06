#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/runtime/Audio.h"

int main() {
    port::AudioRing ring(8);
    assert(port::AudioRing::SAMPLE_RATE == 48000);
    assert(port::AudioRing::CHANNELS == 2);

    // push 4 frames stereo e pop igual
    int16_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(ring.push(in, 4) == 4);
    assert(ring.pending() == 4);
    int16_t out[8] = {};
    assert(ring.pop(out, 4) == 4);
    for (int i = 0; i < 8; ++i) assert(out[i] == in[i]);
    assert(ring.pending() == 0);

    // overflow: só cabe a capacidade, resto conta como dropped
    std::vector<int16_t> big(8 * 2 * 4, 100); // 32 frames
    assert(ring.push(big.data(), 32) == 8);
    assert(ring.dropped() == 24);

    // pop parcial mantém ordem (FIFO)
    int16_t p[4] = {};
    assert(ring.pop(p, 2) == 2);
    assert(p[0] == 100 && p[3] == 100);
    assert(ring.pending() == 6);

    ring.clear();
    assert(ring.pending() == 0);

    std::printf("audio tests passed!\n");
    return 0;
}
