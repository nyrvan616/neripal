#pragma once

#include "neripal/persist/IWallClock.hpp"

#include <cstdint>
#include <optional>

class FakeWallClock final : public neripal::persist::IWallClock {
public:
    std::optional<std::int64_t> nowUnixSeconds() const override { return now_; }

    void set(std::int64_t unixSeconds) { now_ = unixSeconds; }
    void clear() { now_.reset(); }

private:
    std::optional<std::int64_t> now_{};
};
