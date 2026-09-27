#pragma once

#include "neripal/persist/ISaveStorage.hpp"

#include <cstdint>

class FakeSaveStorage final : public neripal::persist::ISaveStorage {
public:
    neripal::persist::StorageStatus readSlot(std::uint8_t slot,
                                             neripal::persist::SaveBytes& out) override {
        if (slot >= neripal::persist::kSaveSlotCount) {
            return neripal::persist::StorageStatus::Error;
        }
        ++reads_[slot];
        if (failRead_[slot]) {
            return neripal::persist::StorageStatus::Error;
        }
        if (!occupied_[slot]) {
            return neripal::persist::StorageStatus::Empty;
        }
        out = slots_[slot];
        return neripal::persist::StorageStatus::Ok;
    }

    neripal::persist::StorageStatus writeSlot(std::uint8_t slot,
                                              const neripal::persist::SaveBytes& in) override {
        if (slot >= neripal::persist::kSaveSlotCount) {
            return neripal::persist::StorageStatus::Error;
        }
        ++writes_[slot];
        if (failWrite_[slot]) {
            return neripal::persist::StorageStatus::Error;
        }
        slots_[slot] = in;
        occupied_[slot] = true;
        if (corruptAfterWrite_[slot] && in.size > 20) {
            slots_[slot].data[20] ^= 0x01;
        }
        return neripal::persist::StorageStatus::Ok;
    }

    void setFailWrite(std::uint8_t slot, bool fail) {
        if (slot < neripal::persist::kSaveSlotCount) {
            failWrite_[slot] = fail;
        }
    }

    void setFailRead(std::uint8_t slot, bool fail) {
        if (slot < neripal::persist::kSaveSlotCount) {
            failRead_[slot] = fail;
        }
    }

    void setCorruptAfterWrite(std::uint8_t slot, bool corrupt) {
        if (slot < neripal::persist::kSaveSlotCount) {
            corruptAfterWrite_[slot] = corrupt;
        }
    }

    int writes(std::uint8_t slot) const {
        return slot < neripal::persist::kSaveSlotCount ? writes_[slot] : 0;
    }

    int totalWrites() const { return writes_[0] + writes_[1]; }

    bool occupied(std::uint8_t slot) const {
        return slot < neripal::persist::kSaveSlotCount && occupied_[slot];
    }

    const neripal::persist::SaveBytes& slot(std::uint8_t slot) const { return slots_[slot]; }

private:
    neripal::persist::SaveBytes slots_[neripal::persist::kSaveSlotCount]{};
    bool occupied_[neripal::persist::kSaveSlotCount]{};
    bool failWrite_[neripal::persist::kSaveSlotCount]{};
    bool failRead_[neripal::persist::kSaveSlotCount]{};
    bool corruptAfterWrite_[neripal::persist::kSaveSlotCount]{};
    int writes_[neripal::persist::kSaveSlotCount]{};
    int reads_[neripal::persist::kSaveSlotCount]{};
};
