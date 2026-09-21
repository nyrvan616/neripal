#pragma once

#include <cstdint>

namespace neripal::core {

class IRandom {
public:
    virtual ~IRandom() = default;
    virtual std::uint32_t nextU32() = 0;

    // Maps one nextU32() sample into [0, n). n == 0 returns 0 and does not consume a sample.
    std::uint32_t nextBounded(std::uint32_t n) {
        if (n == 0u) {
            return 0u;
        }
        return nextU32() % n;
    }
};

}  // namespace neripal::core
