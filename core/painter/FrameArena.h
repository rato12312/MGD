#pragma once

#include <vector>
#include <cstdint>

namespace mgd {

class FrameArena {
    std::vector<uint8_t> memory;
    size_t offset = 0;

public:
    FrameArena() : memory(1024 * 1024, 0) {}
    explicit FrameArena(size_t size) : memory(size, 0) {}

    void* alloc(size_t bytes) {
        size_t aligned = (offset + 7) & ~size_t(7);
        if (aligned + bytes > memory.size()) return nullptr;
        void* ptr = memory.data() + aligned;
        offset = aligned + bytes;
        return ptr;
    }

    void reset() { offset = 0; }
    size_t used() const { return offset; }
    size_t capacity() const { return memory.size(); }
};

} // namespace mgd
