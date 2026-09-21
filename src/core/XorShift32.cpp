#include "neripal/core/XorShift32.hpp"

namespace neripal::core {

namespace {
// Internal substitute so a zero seed cannot collapse the generator to a zero stream.
// This is not a composition-root seed and is not firmware policy.
constexpr std::uint32_t kZeroSeedSubstitute = 0x9E3779B9u;
}

XorShift32::XorShift32(std::uint32_t seed)
    : state_(seed == 0u ? kZeroSeedSubstitute : seed) {}

std::uint32_t XorShift32::nextU32() {
    std::uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
}

}  // namespace neripal::core
