#pragma once

#include <cstdint>
#include <optional>

namespace neripal::persist {

class IWallClock {
public:
    virtual ~IWallClock() = default;
    // nullopt if calendar time is not trustworthy.
    virtual std::optional<std::int64_t> nowUnixSeconds() const = 0;
};

}  // namespace neripal::persist
