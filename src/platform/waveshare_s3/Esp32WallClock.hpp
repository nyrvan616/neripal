#pragma once

#include "neripal/persist/IWallClock.hpp"

namespace neripal::waveshare_s3 {

class Esp32WallClock final : public persist::IWallClock {
public:
    std::optional<std::int64_t> nowUnixSeconds() const override;
};

}  // namespace neripal::waveshare_s3
