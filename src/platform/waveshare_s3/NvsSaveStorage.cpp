#include "NvsSaveStorage.hpp"

#include <cstddef>
#include <nvs.h>
#include <nvs_flash.h>

namespace neripal::waveshare_s3 {
namespace {

constexpr const char* kNamespace = "neripal";

// partitions_16mb.csv reserves 0x5000 (20 KiB) for NVS. ESP-IDF keeps one 4 KiB
// page as state, and nvs_set_blob accepts a value up to roughly 15 KiB on that
// partition. Two V2 slots of 233 bytes, plus the namespace entry, stay inside
// one data page even before counting entry headers.
static_assert(neripal::persist::kSaveV2BlobBytes <= neripal::persist::kSaveBlobCapacity);
static_assert(neripal::persist::kSaveBlobCapacity <= 1024);
static_assert(2 * neripal::persist::kSaveBlobCapacity < 0x1000);

}  // namespace

const char* NvsSaveStorage::slotKey(std::uint8_t slot) noexcept {
    return slot == 0 ? "slot0" : "slot1";
}

bool NvsSaveStorage::begin() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) {
            ready_ = false;
            return false;
        }
        err = nvs_flash_init();
    }
    ready_ = (err == ESP_OK);
    return ready_;
}

persist::StorageStatus NvsSaveStorage::readSlot(std::uint8_t slot, persist::SaveBytes& out) {
    if (!ready_ || slot >= persist::kSaveSlotCount) {
        return persist::StorageStatus::Error;
    }
    nvs_handle_t handle = 0;
    const auto open = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open == ESP_ERR_NVS_NOT_FOUND) {
        return persist::StorageStatus::Empty;
    }
    if (open != ESP_OK) {
        return persist::StorageStatus::Error;
    }
    size_t length = persist::kSaveBlobCapacity;
    const auto err = nvs_get_blob(handle, slotKey(slot), out.data, &length);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return persist::StorageStatus::Empty;
    }
    if (err != ESP_OK || length == 0 || length > persist::kSaveBlobCapacity) {
        return persist::StorageStatus::Error;
    }
    out.size = static_cast<std::uint16_t>(length);
    return persist::StorageStatus::Ok;
}

persist::StorageStatus NvsSaveStorage::writeSlot(std::uint8_t slot, const persist::SaveBytes& in) {
    if (!ready_ || slot >= persist::kSaveSlotCount || in.size == 0 ||
        in.size > persist::kSaveBlobCapacity) {
        return persist::StorageStatus::Error;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return persist::StorageStatus::Error;
    }
    const auto set = nvs_set_blob(handle, slotKey(slot), in.data, in.size);
    const auto commit = (set == ESP_OK) ? nvs_commit(handle) : set;
    nvs_close(handle);
    return commit == ESP_OK ? persist::StorageStatus::Ok : persist::StorageStatus::Error;
}

}  // namespace neripal::waveshare_s3
