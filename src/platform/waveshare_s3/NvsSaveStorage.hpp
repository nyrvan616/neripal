#pragma once

#include "neripal/persist/ISaveStorage.hpp"

namespace neripal::waveshare_s3 {

class NvsSaveStorage final : public persist::ISaveStorage {
public:
    bool begin();
    persist::StorageStatus readSlot(std::uint8_t slot, persist::SaveBytes& out) override;
    persist::StorageStatus writeSlot(std::uint8_t slot, const persist::SaveBytes& in) override;

private:
    static const char* slotKey(std::uint8_t slot) noexcept;
    bool ready_ = false;
};

}  // namespace neripal::waveshare_s3
