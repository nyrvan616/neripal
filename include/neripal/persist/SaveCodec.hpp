#pragma once

#include "neripal/core/PetSnapshot.hpp"

#include <cstddef>
#include <cstdint>

namespace neripal::persist {

inline constexpr std::uint32_t kSaveMagic = 0x4E503031u;  // 'NP01'
inline constexpr std::uint16_t kSaveVersionV1 = 1;
inline constexpr std::uint16_t kSaveV1PayloadBytes = 44;
inline constexpr std::uint16_t kSaveV1BlobBytes = 56;
inline constexpr std::size_t kSaveBlobCapacity = 64;

enum class DecodeStatus : std::uint8_t {
    Ok = 0,
    Truncated,
    BadMagic,
    UnsupportedVersion,
    BadLength,
    BadChecksum,
    InvalidStage,
    InvalidSleepCause,
};

struct SaveRecord {
    std::uint32_t sequence = 1;
    std::int64_t savedUnixSeconds = 0;
    core::PetSnapshot snapshot{};
};

struct DecodeResult {
    DecodeStatus status = DecodeStatus::Truncated;
    SaveRecord record{};
};

bool encodeV1(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept;
DecodeResult decode(const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace neripal::persist
