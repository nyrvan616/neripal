#pragma once

#include "neripal/persist/ISaveStorage.hpp"

#include <filesystem>

namespace neripal::desktop {

class FileSaveStorage final : public persist::ISaveStorage {
public:
    explicit FileSaveStorage(std::filesystem::path directory);

    const std::filesystem::path& directory() const noexcept { return directory_; }
    std::filesystem::path slotPath(std::uint8_t slot) const;

    persist::StorageStatus readSlot(std::uint8_t slot, persist::SaveBytes& out) override;
    persist::StorageStatus writeSlot(std::uint8_t slot, const persist::SaveBytes& in) override;

private:
    std::filesystem::path tmpPath(std::uint8_t slot) const;
    static bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to);

    std::filesystem::path directory_;
};

}  // namespace neripal::desktop
