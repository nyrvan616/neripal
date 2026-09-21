#pragma once

#include "neripal/core/IRandom.hpp"

#include <cstdint>

namespace neripal::core {

class XorShift32 final : public IRandom {
public:
    explicit XorShift32(std::uint32_t seed);

    std::uint32_t nextU32() override;

private:
    std::uint32_t state_;
};

}  // namespace neripal::core
