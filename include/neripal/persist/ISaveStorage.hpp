#pragma once

#include "neripal/persist/SaveCodec.hpp"

#include <cstdint>

namespace neripal::persist {

enum class StorageStatus : std::uint8_t {
    Ok = 0,
    Empty,
    Error,
};

inline constexpr std::uint8_t kSaveSlotCount = 2;

struct SaveBytes {
    std::uint8_t data[kSaveBlobCapacity]{};
    std::uint16_t size = 0;
};

class ISaveStorage {
public:
    virtual ~ISaveStorage() = default;
    virtual StorageStatus readSlot(std::uint8_t slot, SaveBytes& out) = 0;
    virtual StorageStatus writeSlot(std::uint8_t slot, const SaveBytes& in) = 0;
};

}  // namespace neripal::persist
