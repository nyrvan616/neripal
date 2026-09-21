#include "neripal/core/Care.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/persist/Crc32.hpp"
#include "neripal/persist/SaveCodec.hpp"
#include "neripal/persist/SaveSession.hpp"

#include "neripal/core/XorShift32.hpp"

#include "FakeClock.hpp"
#include "FakeRandom.hpp"
#include "FakeSaveStorage.hpp"
#include "FakeWallClock.hpp"

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using neripal::core::EvolutionStage;
using neripal::core::Pet;
using neripal::core::PetSnapshot;
using neripal::core::SleepCause;
using neripal::core::XorShift32;
using neripal::persist::BootResult;
using neripal::persist::DecodeStatus;
using neripal::persist::SaveBytes;
using neripal::persist::SaveRecord;
using neripal::persist::SaveSession;
using neripal::persist::StorageStatus;

struct TestCase {
    std::string_view name;
    std::function<bool()> run;
};

bool snapshotsEqual(const PetSnapshot& a, const PetSnapshot& b) {
    return a.hunger == b.hunger && a.happiness == b.happiness && a.energy == b.energy &&
           a.health == b.health && a.hygiene == b.hygiene && a.ageMillis == b.ageMillis &&
           a.needsRemainderMs == b.needsRemainderMs && a.stage == b.stage &&
           a.sleepCause == b.sleepCause && a.napRemainingMs == b.napRemainingMs;
}

bool encodeOk(const SaveRecord& record, std::uint8_t* blob, std::uint16_t& written) {
    return neripal::persist::encodeV1(record, blob, neripal::persist::kSaveBlobCapacity, written) &&
           written == neripal::persist::kSaveV1BlobBytes;
}

bool crc32KnownVector() {
    const char* text = "123456789";
    return neripal::persist::crc32(reinterpret_cast<const std::uint8_t*>(text), 9) == 0xCBF43926u;
}

bool codecRoundtripNone() {
    SaveRecord record;
    record.sequence = 7;
    record.savedUnixSeconds = 1'700'000'000;
    record.snapshot.hunger = 10;
    record.snapshot.happiness = 20;
    record.snapshot.energy = 30;
    record.snapshot.health = 40;
    record.snapshot.hygiene = 50;
    record.snapshot.ageMillis = 123456789ull;
    record.snapshot.needsRemainderMs = 45'000;
    record.snapshot.stage = EvolutionStage::Child;
    record.snapshot.sleepCause = SleepCause::None;
    record.snapshot.napRemainingMs = 0;

    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    const auto decoded = neripal::persist::decode(blob, written);
    return decoded.status == DecodeStatus::Ok && decoded.record.sequence == 7 &&
           decoded.record.savedUnixSeconds == 1'700'000'000 &&
           snapshotsEqual(decoded.record.snapshot, record.snapshot);
}

bool codecRoundtripPlayerAndNap() {
    SaveRecord player;
    player.sequence = 1;
    player.snapshot.sleepCause = SleepCause::Player;
    player.snapshot.napRemainingMs = 0;
    SaveRecord nap;
    nap.sequence = 2;
    nap.snapshot.sleepCause = SleepCause::Nap;
    nap.snapshot.napRemainingMs = 9'000;

    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(player, blob, written)) return false;
    auto decoded = neripal::persist::decode(blob, written);
    if (decoded.status != DecodeStatus::Ok ||
        decoded.record.snapshot.sleepCause != SleepCause::Player) {
        return false;
    }
    if (!encodeOk(nap, blob, written)) return false;
    decoded = neripal::persist::decode(blob, written);
    return decoded.status == DecodeStatus::Ok &&
           decoded.record.snapshot.sleepCause == SleepCause::Nap &&
           decoded.record.snapshot.napRemainingMs == 9'000;
}

bool codecIsLittleEndian() {
    SaveRecord record;
    record.sequence = 0x01020304u;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    return blob[12] == 0x04 && blob[13] == 0x03 && blob[14] == 0x02 && blob[15] == 0x01;
}

bool codecRejectsAlteredByte() {
    SaveRecord record;
    record.sequence = 3;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[20] ^= 0x01;
    return neripal::persist::decode(blob, written).status == DecodeStatus::BadChecksum;
}

bool codecRejectsTruncated() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    return neripal::persist::decode(blob, 11).status == DecodeStatus::Truncated &&
           neripal::persist::decode(blob, written - 1).status == DecodeStatus::Truncated;
}

bool codecRejectsBadMagic() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[0] ^= 0xFF;
    return neripal::persist::decode(blob, written).status == DecodeStatus::BadMagic;
}

bool codecRejectsUnknownVersion() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[4] = 2;
    blob[5] = 0;
    return neripal::persist::decode(blob, written).status == DecodeStatus::UnsupportedVersion;
}

bool codecRejectsBadPayloadLength() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[6] = 40;
    blob[7] = 0;
    return neripal::persist::decode(blob, written).status == DecodeStatus::BadLength;
}

void rewriteCrc(std::uint8_t* blob) {
    const auto crc = neripal::persist::crc32(blob + 12, neripal::persist::kSaveV1PayloadBytes);
    blob[8] = static_cast<std::uint8_t>(crc);
    blob[9] = static_cast<std::uint8_t>(crc >> 8);
    blob[10] = static_cast<std::uint8_t>(crc >> 16);
    blob[11] = static_cast<std::uint8_t>(crc >> 24);
}

bool codecRejectsInvalidStage() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[12 + 34] = 99;
    rewriteCrc(blob);
    return neripal::persist::decode(blob, written).status == DecodeStatus::InvalidStage;
}

bool codecRejectsInvalidSleepCause() {
    SaveRecord record;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[12 + 35] = 9;
    rewriteCrc(blob);
    return neripal::persist::decode(blob, written).status == DecodeStatus::InvalidSleepCause;
}

bool codecWritesExplicitLayout() {
    SaveRecord record;
    record.sequence = 1;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    return written == 56 && blob[0] == 0x31 && blob[1] == 0x30 && blob[2] == 0x50 &&
           blob[3] == 0x4E && blob[12 + 40] == 0 && blob[12 + 41] == 0 &&
           blob[12 + 42] == 0 && blob[12 + 43] == 0;
}

bool petCaptureCodecRoundtrip() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    pet.sleep();
    SaveRecord record;
    record.sequence = 4;
    record.savedUnixSeconds = 42;
    record.snapshot = pet.capture();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    const auto decoded = neripal::persist::decode(blob, written);
    if (decoded.status != DecodeStatus::Ok) return false;
    Pet other(clock, rng);
    other.restoreSnapshot(decoded.record.snapshot);
    return snapshotsEqual(other.capture(), record.snapshot) && other.state().sleeping;
}

struct SessionEnv {
    FakeClock game{};
    FakeClock session{};
    XorShift32 rng{1u};
    FakeWallClock wall{};
    FakeSaveStorage store{};
    Pet pet{game, rng};
    SaveSession saves{pet, store, wall, session};

    bool put(std::uint8_t slot, std::uint32_t sequence, std::int64_t unixSeconds,
             PetSnapshot snapshot = {}) {
        SaveRecord record;
        record.sequence = sequence;
        record.savedUnixSeconds = unixSeconds;
        record.snapshot = snapshot;
        SaveBytes bytes{};
        std::uint16_t written = 0;
        if (!encodeOk(record, bytes.data, written)) return false;
        bytes.size = written;
        return store.writeSlot(slot, bytes) == StorageStatus::Ok;
    }

    bool slotSequence(std::uint8_t slot, std::uint32_t& sequence) const {
        if (!store.occupied(slot)) return false;
        const auto decoded = neripal::persist::decode(store.slot(slot).data, store.slot(slot).size);
        if (decoded.status != DecodeStatus::Ok) return false;
        sequence = decoded.record.sequence;
        return true;
    }
};

bool sessionNewerSlotWins() {
    SessionEnv env;
    PetSnapshot older;
    older.hunger = 10;
    PetSnapshot newer;
    newer.hunger = 20;
    if (!env.put(0, 1, 1000, older) || !env.put(1, 4, 1000, newer)) return false;
    env.wall.set(1000);
    return env.saves.boot() == BootResult::Restored && env.pet.state().hunger == 20 &&
           env.saves.nextSequence() == 5;
}

bool sessionCorruptNewerFallsToBackup() {
    SessionEnv env;
    PetSnapshot backup;
    backup.hunger = 11;
    PetSnapshot newer;
    newer.hunger = 77;
    if (!env.put(0, 1, 1000, backup) || !env.put(1, 9, 1000, newer)) return false;
    auto damaged = env.store.slot(1);
    damaged.data[20] ^= 0xFF;
    if (env.store.writeSlot(1, damaged) != StorageStatus::Ok) return false;
    env.wall.set(1000);
    return env.saves.boot() == BootResult::RestoredFromBackup && env.pet.state().hunger == 11;
}

bool sessionFailedWriteDoesNotAdvanceSequence() {
    SessionEnv env;
    env.wall.set(1000);
    if (env.saves.boot() != BootResult::Fresh) return false;
    env.store.setFailWrite(0, true);
    const auto before = env.saves.nextSequence();
    if (env.saves.saveNow()) return false;
    env.store.setFailWrite(0, false);
    if (env.saves.nextSequence() != before) return false;
    if (!env.saves.saveNow()) return false;
    std::uint32_t seq = 0;
    return env.saves.nextSequence() == before + 1 && env.slotSequence(0, seq) && seq == before;
}

bool sessionInvalidReadbackKeepsPreviousSlot() {
    SessionEnv env;
    PetSnapshot first;
    first.hunger = 12;
    if (!env.put(0, 1, 1000, first)) return false;
    env.wall.set(1000);
    if (env.saves.boot() != BootResult::Restored) return false;
    env.pet.feed();
    env.store.setCorruptAfterWrite(1, true);
    const auto next = env.saves.nextSequence();
    if (env.saves.saveNow()) return false;
    if (env.saves.nextSequence() != next) return false;
    std::uint32_t seq0 = 0;
    return env.slotSequence(0, seq0) && seq0 == 1 && env.pet.capture().hunger != first.hunger;
}

bool sessionBothInvalidStayFreshWithoutErase() {
    SessionEnv env;
    PetSnapshot snap;
    if (!env.put(0, 1, 1000, snap) || !env.put(1, 2, 1000, snap)) return false;
    auto a = env.store.slot(0);
    auto b = env.store.slot(1);
    a.data[8] ^= 0x01;
    b.data[8] ^= 0x01;
    if (env.store.writeSlot(0, a) != StorageStatus::Ok) return false;
    if (env.store.writeSlot(1, b) != StorageStatus::Ok) return false;
    const auto bytes0 = env.store.slot(0);
    const auto bytes1 = env.store.slot(1);
    const int writesBeforeBoot = env.store.totalWrites();
    env.wall.set(1000);
    if (env.saves.boot() != BootResult::Fresh) return false;
    return env.store.totalWrites() == writesBeforeBoot &&
           env.store.slot(0).size == bytes0.size && env.store.slot(1).size == bytes1.size &&
           env.store.slot(0).data[8] == bytes0.data[8] && env.store.slot(1).data[8] == bytes1.data[8];
}

bool sessionTrustedBootAppliesOfflineOnce() {
    SessionEnv env;
    PetSnapshot snap;
    snap.hunger = 10;
    if (!env.put(0, 1, 1'000, snap)) return false;
    env.wall.set(1'000 + 120);
    if (env.saves.boot() != BootResult::Restored) return false;
    const auto afterFirst = env.pet.capture();
    if (afterFirst.ageMillis != 120'000 || afterFirst.hunger != 12) return false;

    FakeClock game2;
    FakeClock session2;
    XorShift32 rng2{2u};
    Pet pet2(game2, rng2);
    SaveSession second(pet2, env.store, env.wall, session2);
    if (second.boot() != BootResult::Restored) return false;
    return pet2.capture().ageMillis == afterFirst.ageMillis &&
           pet2.capture().hunger == afterFirst.hunger && second.savedUnixSeconds() == 1'120;
}

bool sessionBootWithoutWallAbandonsAnchor() {
    SessionEnv env;
    PetSnapshot snap;
    snap.ageMillis = 5'000;
    if (!env.put(0, 3, 50'000, snap)) return false;
    if (env.saves.boot() != BootResult::Restored) return false;
    if (env.saves.savedUnixSeconds() != 0 || env.pet.capture().ageMillis != 5'000) return false;

    env.wall.set(80'000);
    env.saves.tick();
    if (env.saves.savedUnixSeconds() != 80'000 || env.pet.capture().ageMillis != 5'000) {
        return false;
    }
    if (!env.saves.saveNow()) return false;

    FakeClock game2;
    FakeClock session2;
    XorShift32 rng2{2u};
    Pet pet2(game2, rng2);
    SaveSession second(pet2, env.store, env.wall, session2);
    if (second.boot() != BootResult::Restored) return false;
    return pet2.capture().ageMillis == 5'000 && second.savedUnixSeconds() == 80'000;
}

bool sessionBackwardsWallDoesNotRewind() {
    SessionEnv env;
    PetSnapshot snap;
    snap.ageMillis = 9'000;
    snap.hunger = 40;
    if (!env.put(0, 1, 8'000, snap)) return false;
    env.wall.set(3'000);
    if (env.saves.boot() != BootResult::Restored) return false;
    return env.pet.capture().ageMillis == 9'000 && env.pet.capture().hunger == 40 &&
           env.saves.savedUnixSeconds() == 3'000;
}

bool sessionWallAppearsMidSessionDoesNotDuplicateRuntime() {
    SessionEnv env;
    PetSnapshot snap;
    snap.ageMillis = 0;
    if (!env.put(0, 1, 1'000, snap)) return false;
    if (env.saves.boot() != BootResult::Restored) return false;
    env.game.advance(10 * 60'000);
    env.pet.update();
    const auto ageAfterPlay = env.pet.capture().ageMillis;
    env.wall.set(1'000 + 24 * 60 * 60);
    env.saves.tick();
    return env.pet.capture().ageMillis == ageAfterPlay && env.saves.savedUnixSeconds() == 1'000 + 24 * 60 * 60;
}

bool sessionCareOnlyMarksDirty() {
    SessionEnv env;
    env.wall.set(1000);
    env.saves.boot();
    const int writes = env.store.totalWrites();
    env.saves.noteCareResult(env.pet.feed());
    env.saves.noteCareResult(env.pet.train());
    env.saves.tick();
    return env.saves.dirty() && env.store.totalWrites() == writes;
}

bool sessionAutosaveWritesWhenDue() {
    SessionEnv env;
    env.wall.set(1000);
    env.saves.boot();
    if (!env.saves.saveNow()) return false;
    const int writes = env.store.totalWrites();
    env.saves.noteCareResult(env.pet.feed());
    env.saves.tick();
    if (env.store.totalWrites() != writes) return false;
    env.session.advance(neripal::persist::kAutosaveSessionMs);
    env.saves.tick();
    return !env.saves.dirty() && env.store.totalWrites() == writes + 1;
}

bool sessionDebounceStopsBursts() {
    SessionEnv env;
    env.wall.set(1000);
    env.saves.boot();
    if (!env.saves.saveNow()) return false;
    env.saves.noteCareResult(env.pet.feed());
    env.session.advance(neripal::persist::kAutosaveSessionMs);
    env.saves.tick();
    const int writes = env.store.totalWrites();
    env.saves.noteCareResult(env.pet.clean());
    env.saves.tick();
    env.session.advance(neripal::persist::kMinSaveIntervalMs - 1);
    env.saves.tick();
    if (env.store.totalWrites() != writes) return false;
    env.session.advance(neripal::persist::kAutosaveSessionMs);
    env.saves.tick();
    return env.store.totalWrites() == writes + 1;
}

bool sessionGameplayClockDoesNotAccelerateAutosave() {
    SessionEnv env;
    env.wall.set(1000);
    env.saves.boot();
    if (!env.saves.saveNow()) return false;
    const int writes = env.store.totalWrites();
    env.saves.noteCareResult(env.pet.feed());
    env.game.advance(1'000ull * neripal::persist::kAutosaveSessionMs);
    env.pet.update();
    env.saves.tick();
    if (env.store.totalWrites() != writes) return false;
    env.session.advance(neripal::persist::kAutosaveSessionMs);
    env.saves.tick();
    return env.store.totalWrites() == writes + 1;
}

}  // namespace

int main() {
    const std::vector<TestCase> tests{
        {"crc32 known vector", crc32KnownVector},
        {"codec roundtrip none", codecRoundtripNone},
        {"codec roundtrip player and nap", codecRoundtripPlayerAndNap},
        {"codec is little endian", codecIsLittleEndian},
        {"codec rejects altered byte", codecRejectsAlteredByte},
        {"codec rejects truncated", codecRejectsTruncated},
        {"codec rejects bad magic", codecRejectsBadMagic},
        {"codec rejects unknown version", codecRejectsUnknownVersion},
        {"codec rejects bad payload length", codecRejectsBadPayloadLength},
        {"codec rejects invalid stage", codecRejectsInvalidStage},
        {"codec rejects invalid sleepCause", codecRejectsInvalidSleepCause},
        {"codec writes explicit layout", codecWritesExplicitLayout},
        {"pet capture codec roundtrip", petCaptureCodecRoundtrip},
        {"session newer slot wins", sessionNewerSlotWins},
        {"session corrupt newer falls to backup", sessionCorruptNewerFallsToBackup},
        {"session failed write does not advance sequence", sessionFailedWriteDoesNotAdvanceSequence},
        {"session invalid readback keeps previous slot", sessionInvalidReadbackKeepsPreviousSlot},
        {"session both invalid stay fresh without erase", sessionBothInvalidStayFreshWithoutErase},
        {"session trusted boot applies offline once", sessionTrustedBootAppliesOfflineOnce},
        {"session boot without wall abandons anchor", sessionBootWithoutWallAbandonsAnchor},
        {"session backwards wall does not rewind", sessionBackwardsWallDoesNotRewind},
        {"session wall appears mid session does not duplicate runtime",
         sessionWallAppearsMidSessionDoesNotDuplicateRuntime},
        {"session care only marks dirty", sessionCareOnlyMarksDirty},
        {"session autosave writes when due", sessionAutosaveWritesWhenDue},
        {"session debounce stops bursts", sessionDebounceStopsBursts},
        {"session gameplay clock does not accelerate autosave",
         sessionGameplayClockDoesNotAccelerateAutosave},
    };

    int failures = 0;
    for (const auto& test : tests) {
        bool passed = false;
        try {
            passed = test.run();
        } catch (const std::exception& ex) {
            std::cout << "[FAIL] " << test.name << " (" << ex.what() << ")\n";
            failures += 1;
            continue;
        }
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += passed ? 0 : 1;
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
