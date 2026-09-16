#pragma once

#include <cstdint>

namespace neripal::core {

class IClock {
public:
    virtual ~IClock() = default;
    virtual std::uint64_t nowMillis() const = 0;
};

}  // namespace neripal::core
