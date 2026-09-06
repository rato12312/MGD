#pragma once

#include <cstdint>
#include <vector>

namespace port {

// Áudio mínimo do port (stereo 48kHz, como no log do boot).
// Ring buffer sem saída real ainda: o jogo escreve samples, o backend
// (a ligar no Android) consome. Testável puro, sem SO.
class AudioRing {
public:
    static constexpr uint32_t SAMPLE_RATE = 48000;
    static constexpr uint32_t CHANNELS = 2;

    explicit AudioRing(size_t capacity_frames = 4096)
        : capacity_(capacity_frames),
          buffer_(capacity_frames * CHANNELS, 0),
          read_(0), write_(0), count_(0) {}

    // Escreve frames intercalados LRLR... Retorna quantos couberam.
    size_t push(const int16_t* frames, size_t n) {
        size_t pushed = 0;
        for (size_t i = 0; i < n && count_ < capacity_; ++i) {
            buffer_[(write_ * CHANNELS)] = frames[i * CHANNELS];
            buffer_[(write_ * CHANNELS) + 1] = frames[i * CHANNELS + 1];
            write_ = (write_ + 1) % capacity_;
            count_++;
            pushed++;
        }
        dropped_ += (n - pushed);
        return pushed;
    }

    // Consome frames para o backend. Retorna quantos saíram.
    size_t pop(int16_t* out, size_t n) {
        size_t got = 0;
        for (size_t i = 0; i < n && count_ > 0; ++i) {
            out[i * CHANNELS] = buffer_[(read_ * CHANNELS)];
            out[i * CHANNELS + 1] = buffer_[(read_ * CHANNELS) + 1];
            read_ = (read_ + 1) % capacity_;
            count_--;
            got++;
        }
        return got;
    }

    size_t pending() const { return count_; }
    size_t capacity() const { return capacity_; }
    uint64_t dropped() const { return dropped_; }
    void clear() { read_ = write_ = count_ = 0; }

private:
    size_t capacity_;
    std::vector<int16_t> buffer_;
    size_t read_ = 0, write_ = 0, count_ = 0;
    uint64_t dropped_ = 0;
};

} // namespace port
