#include "FileSaveStorage.hpp"

#include <fstream>
#include <string>

namespace neripal::desktop {
namespace {

std::string slotName(std::uint8_t slot) {
    return slot == 0 ? "neripal-slot0.bin" : "neripal-slot1.bin";
}

}  // namespace

FileSaveStorage::FileSaveStorage(std::filesystem::path directory)
    : directory_(std::move(directory)) {
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
}

std::filesystem::path FileSaveStorage::slotPath(std::uint8_t slot) const {
    return directory_ / slotName(slot);
}

std::filesystem::path FileSaveStorage::tmpPath(std::uint8_t slot) const {
    return directory_ / (slotName(slot) + ".tmp");
}

bool FileSaveStorage::replaceFile(const std::filesystem::path& from,
                                  const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (!ec) {
        return true;
    }
    std::filesystem::remove(to, ec);
    std::filesystem::rename(from, to, ec);
    return !ec;
}

persist::StorageStatus FileSaveStorage::readSlot(std::uint8_t slot, persist::SaveBytes& out) {
    if (slot >= persist::kSaveSlotCount) {
        return persist::StorageStatus::Error;
    }
    const auto path = slotPath(slot);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return persist::StorageStatus::Empty;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return persist::StorageStatus::Error;
    }
    in.read(reinterpret_cast<char*>(out.data), static_cast<std::streamsize>(persist::kSaveBlobCapacity));
    const auto got = in.gcount();
    if (got <= 0) {
        return persist::StorageStatus::Empty;
    }
    if (got > static_cast<std::streamsize>(persist::kSaveBlobCapacity)) {
        return persist::StorageStatus::Error;
    }
    out.size = static_cast<std::uint16_t>(got);
    return persist::StorageStatus::Ok;
}

persist::StorageStatus FileSaveStorage::writeSlot(std::uint8_t slot, const persist::SaveBytes& in) {
    if (slot >= persist::kSaveSlotCount || in.size == 0 || in.size > persist::kSaveBlobCapacity) {
        return persist::StorageStatus::Error;
    }
    const auto tmp = tmpPath(slot);
    const auto dest = slotPath(slot);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return persist::StorageStatus::Error;
        }
        out.write(reinterpret_cast<const char*>(in.data), static_cast<std::streamsize>(in.size));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return persist::StorageStatus::Error;
        }
    }
    if (!replaceFile(tmp, dest)) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return persist::StorageStatus::Error;
    }
    return persist::StorageStatus::Ok;
}

}  // namespace neripal::desktop
