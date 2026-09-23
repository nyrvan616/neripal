#pragma once

#include "neripal/persist/IWallClock.hpp"

namespace neripal::desktop {

class SystemWallClock final : public persist::IWallClock {
public:
    std::optional<std::int64_t> nowUnixSeconds() const override;
};

}  // namespace neripal::desktop
