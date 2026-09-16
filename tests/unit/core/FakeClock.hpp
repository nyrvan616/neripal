#pragma once

#include "neripal/core/IClock.hpp"

#include <cstdint>

class FakeClock final : public neripal::core::IClock {
public:
    std::uint64_t nowMillis() const override { return nowMs_; }
    void advance(std::uint64_t milliseconds) { nowMs_ += milliseconds; }
    void set(std::uint64_t milliseconds) { nowMs_ = milliseconds; }

private:
    std::uint64_t nowMs_ = 0;
};
