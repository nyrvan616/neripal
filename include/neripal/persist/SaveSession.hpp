#pragma once

#include "neripal/core/Care.hpp"
#include "neripal/core/IClock.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/persist/ISaveStorage.hpp"
#include "neripal/persist/IWallClock.hpp"

#include <cstdint>

namespace neripal::persist {

inline constexpr std::uint64_t kAutosaveSessionMs = 60'000;
inline constexpr std::uint64_t kMinSaveIntervalMs = 5'000;

enum class BootResult : std::uint8_t {
    Fresh = 0,
    Restored,
    RestoredFromBackup,
};

class SaveSession {
public:
    SaveSession(core::Pet& pet, ISaveStorage& storage, IWallClock& wallClock,
                core::IClock& sessionClock);

    BootResult boot();

    // Marks the in-memory pet as already simulated through the current trusted
    // wall-clock second. No flash write. No-op when the wall clock is untrusted.
    void accountWallClockNow();

    // Reuses applyOfflinePolicy for the gap since accountWallClockNow().
    // True only when that policy applied a positive gap and anchored the pet.
    // False does not apply time; the caller may pass a measured interval to
    // Pet::applyOffline.
    bool catchUpAccountedWallClock();

    void noteCareResult(core::CareResult result);
    void markDirty();
    void tick();
    bool saveNow();

    BootResult bootResult() const noexcept { return bootResult_; }
    bool dirty() const noexcept { return dirty_; }
    std::int64_t savedUnixSeconds() const noexcept { return savedUnixSeconds_; }
    std::uint32_t nextSequence() const noexcept { return nextSequence_; }

    static bool sameSnapshot(const core::PetSnapshot& a, const core::PetSnapshot& b) noexcept {
        return snapshotsEqual(a, b);
    }

private:
    struct SlotView {
        bool present = false;
        bool valid = false;
        SaveRecord record{};
    };

    void readSlots(SlotView out[kSaveSlotCount]);
    std::uint8_t targetSlot() const noexcept;
    void adoptWallClockIfNeeded();
    void applyOfflinePolicy(std::int64_t loadedUnix);
    bool writeRecord();
    static std::uint64_t gapMillis(std::int64_t from, std::int64_t to) noexcept;

    core::Pet& pet_;
    ISaveStorage& storage_;
    IWallClock& wallClock_;
    core::IClock& sessionClock_;

    BootResult bootResult_ = BootResult::Fresh;
    bool dirty_ = false;
    std::int64_t savedUnixSeconds_ = 0;
    std::uint32_t nextSequence_ = 1;
    std::uint8_t stickySlot_ = 0xFF;
    std::uint32_t slotSequence_[kSaveSlotCount]{};
    bool slotValid_[kSaveSlotCount]{};
    std::uint64_t lastSuccessMs_ = 0;
    std::uint64_t lastAttemptMs_ = 0;
};

}  // namespace neripal::persist
