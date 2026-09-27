#include "neripal/persist/SaveSession.hpp"

#include "neripal/persist/SaveCodec.hpp"

#include <limits>

namespace neripal::persist {
namespace {

constexpr std::uint8_t kNoSticky = 0xFF;

}  // namespace

SaveSession::SaveSession(core::Pet& pet, ISaveStorage& storage, IWallClock& wallClock,
                         core::IClock& sessionClock)
    : pet_(pet), storage_(storage), wallClock_(wallClock), sessionClock_(sessionClock) {}

std::uint64_t SaveSession::gapMillis(std::int64_t from, std::int64_t to) noexcept {
    if (to <= from) {
        return 0;
    }
    const auto seconds = static_cast<std::uint64_t>(to - from);
    if (seconds > std::numeric_limits<std::uint64_t>::max() / 1000ull) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return seconds * 1000ull;
}

void SaveSession::readSlots(SlotView out[kSaveSlotCount]) {
    for (std::uint8_t slot = 0; slot < kSaveSlotCount; ++slot) {
        out[slot] = SlotView{};
        SaveBytes bytes{};
        const auto status = storage_.readSlot(slot, bytes);
        if (status == StorageStatus::Empty) {
            continue;
        }
        if (status != StorageStatus::Ok || bytes.size == 0) {
            out[slot].present = true;
            continue;
        }
        out[slot].present = true;
        const auto decoded = decode(bytes.data, bytes.size);
        if (decoded.status != DecodeStatus::Ok) {
            continue;
        }
        out[slot].valid = true;
        out[slot].record = decoded.record;
    }
}

std::uint8_t SaveSession::targetSlot() const noexcept {
    if (stickySlot_ < kSaveSlotCount) {
        return stickySlot_;
    }
    if (!slotValid_[0]) {
        return 0;
    }
    if (!slotValid_[1]) {
        return 1;
    }
    return slotSequence_[0] <= slotSequence_[1] ? 0 : 1;
}

bool SaveSession::writeRecord() {
    lastAttemptMs_ = sessionClock_.nowMillis();

    SaveRecord record;
    record.sequence = nextSequence_;
    record.savedUnixSeconds = savedUnixSeconds_;
    record.snapshot = pet_.capture();

    SaveBytes bytes{};
    std::uint16_t written = 0;
    if (!encodeV2(record, bytes.data, kSaveBlobCapacity, written)) {
        return false;
    }
    bytes.size = written;

    const std::uint8_t slot = targetSlot();
    if (storage_.writeSlot(slot, bytes) != StorageStatus::Ok) {
        stickySlot_ = slot;
        return false;
    }

    SaveBytes readback{};
    const auto readStatus = storage_.readSlot(slot, readback);
    if (readStatus != StorageStatus::Ok) {
        stickySlot_ = slot;
        slotValid_[slot] = false;
        return false;
    }
    const auto decoded = decode(readback.data, readback.size);
    if (decoded.status != DecodeStatus::Ok || decoded.record.sequence != record.sequence ||
        decoded.record.savedUnixSeconds != record.savedUnixSeconds ||
        !sameSnapshot(decoded.record.snapshot, record.snapshot)) {
        stickySlot_ = slot;
        slotValid_[slot] = false;
        return false;
    }

    stickySlot_ = kNoSticky;
    slotValid_[slot] = true;
    slotSequence_[slot] = record.sequence;
    nextSequence_ = record.sequence + 1u;
    lastSuccessMs_ = lastAttemptMs_;
    dirty_ = false;
    return true;
}

void SaveSession::adoptWallClockIfNeeded() {
    if (savedUnixSeconds_ != 0) {
        return;
    }
    const auto wall = wallClock_.nowUnixSeconds();
    if (!wall.has_value()) {
        return;
    }
    savedUnixSeconds_ = *wall;
    dirty_ = true;
}

void SaveSession::accountWallClockNow() {
    const auto wall = wallClock_.nowUnixSeconds();
    if (!wall.has_value()) {
        return;
    }
    savedUnixSeconds_ = *wall;
}

bool SaveSession::catchUpAccountedWallClock() {
    const auto wall = wallClock_.nowUnixSeconds();
    if (!wall.has_value() || savedUnixSeconds_ <= 0 || *wall <= savedUnixSeconds_) {
        return false;
    }
    applyOfflinePolicy(savedUnixSeconds_);
    return true;
}

void SaveSession::applyOfflinePolicy(std::int64_t loadedUnix) {
    const auto wall = wallClock_.nowUnixSeconds();
    if (!wall.has_value()) {
        savedUnixSeconds_ = 0;
        if (loadedUnix != 0) {
            saveNow();
        }
        return;
    }

    const auto now = *wall;
    if (loadedUnix > 0 && now > loadedUnix) {
        const auto gap = gapMillis(loadedUnix, now);
        pet_.applyOffline(gap, gap);
        savedUnixSeconds_ = now;
        saveNow();
        return;
    }

    savedUnixSeconds_ = now;
    if (loadedUnix != now) {
        saveNow();
    }
}

BootResult SaveSession::boot() {
    lastSuccessMs_ = sessionClock_.nowMillis();
    lastAttemptMs_ = lastSuccessMs_;
    dirty_ = false;
    stickySlot_ = kNoSticky;
    nextSequence_ = 1;
    savedUnixSeconds_ = 0;
    slotValid_[0] = false;
    slotValid_[1] = false;
    slotSequence_[0] = 0;
    slotSequence_[1] = 0;

    SlotView slots[kSaveSlotCount];
    readSlots(slots);

    int validCount = 0;
    std::uint8_t best = 0xFF;
    std::uint8_t backupHint = 0xFF;
    for (std::uint8_t slot = 0; slot < kSaveSlotCount; ++slot) {
        slotValid_[slot] = slots[slot].valid;
        slotSequence_[slot] = slots[slot].valid ? slots[slot].record.sequence : 0;
        if (!slots[slot].valid) {
            if (slots[slot].present) {
                backupHint = slot;
            }
            continue;
        }
        ++validCount;
        if (best == 0xFF || slots[slot].record.sequence > slots[best].record.sequence) {
            best = slot;
        }
    }

    if (validCount == 0) {
        bootResult_ = BootResult::Fresh;
        const auto wall = wallClock_.nowUnixSeconds();
        savedUnixSeconds_ = wall.value_or(0);
        return bootResult_;
    }

    const bool fromBackup = validCount == 1 && backupHint != 0xFF;
    bootResult_ = fromBackup ? BootResult::RestoredFromBackup : BootResult::Restored;
    const auto& loaded = slots[best].record;
    nextSequence_ = loaded.sequence + 1u;
    pet_.restoreSnapshot(loaded.snapshot);
    applyOfflinePolicy(loaded.savedUnixSeconds);
    return bootResult_;
}

void SaveSession::noteCareResult(core::CareResult result) {
    if (result == core::CareResult::Applied) {
        dirty_ = true;
    }
}

void SaveSession::markDirty() {
    dirty_ = true;
}

void SaveSession::tick() {
    adoptWallClockIfNeeded();
    if (!dirty_) {
        return;
    }
    const auto now = sessionClock_.nowMillis();
    if (now < lastSuccessMs_ || now - lastSuccessMs_ < kAutosaveSessionMs) {
        return;
    }
    if (now < lastAttemptMs_ || now - lastAttemptMs_ < kMinSaveIntervalMs) {
        return;
    }
    writeRecord();
}

bool SaveSession::saveNow() {
    return writeRecord();
}

}  // namespace neripal::persist
