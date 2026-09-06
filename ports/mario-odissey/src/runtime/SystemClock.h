#pragma once

#include <cstdint>
#include <string>

namespace port {

// Relógio mínimo do port (SET/Time): o boot consulta relógio steady,
// contexto do usuário e fuso logo no início (visto no log).
// Valores fixos e sãos; correção automática sempre ligada (como no log).
struct SteadyClockContext {
    uint64_t ticks = 0; // ticks desde boot ( incrementado por update )
};

struct UserClockContext {
    int64_t unix_time = 1700000000; // época fixa determinística
    uint32_t steady_time_point = 0;
};

class SystemClock {
public:
    SystemClock() = default;

    // Avança o relógio steady em ticks (chamado por frame).
    void tick(uint64_t dt_ticks) { steady_.ticks += dt_ticks; }

    SteadyClockContext steady() const { return steady_; }

    UserClockContext userClock() const {
        UserClockContext ctx;
        ctx.unix_time = base_unix_time_ + static_cast<int64_t>(steady_.ticks / ticks_per_second_);
        ctx.steady_time_point = static_cast<uint32_t>(steady_.ticks & 0xFFFFFFFFu);
        return ctx;
    }

    bool automaticCorrectionEnabled() const { return true; }
    std::string timeZoneName() const { return "Etc/GMT"; }
    std::string locationName() const { return location_; }
    void setLocation(const std::string& loc) { location_ = loc; }

private:
    static constexpr uint64_t ticks_per_second_ = 19200000ull; // Switch: 19.2MHz
    SteadyClockContext steady_{1000};
    int64_t base_unix_time_ = 1700000000;
    std::string location_ = "Etc/GMT";
};

} // namespace port
