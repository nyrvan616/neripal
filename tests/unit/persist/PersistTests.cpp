#include "neripal/core/Care.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/persist/Crc32.hpp"
#include "neripal/persist/SaveCodec.hpp"
#include "neripal/persist/SaveSession.hpp"

#include "neripal/core/XorShift32.hpp"
#include "platform/desktop/FileSaveStorage.hpp"
#include "platform/desktop/SystemWallClock.hpp"

#include "FakeClock.hpp"
#include "FakeRandom.hpp"
#include "FakeSaveStorage.hpp"
#include "FakeWallClock.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
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

void hatch(Pet& pet) {
    auto state = pet.state();
    state.stage = EvolutionStage::Baby;
    state.form = neripal::core::FormId::Juvenile;
    pet.restore(state);
}

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
    blob[4] = 3;
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
    const auto payloadBytes = static_cast<std::uint16_t>(blob[6] | (static_cast<std::uint16_t>(blob[7]) << 8));
    const auto crc = neripal::persist::crc32(blob + 12, payloadBytes);
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
    hatch(pet);
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

    SessionEnv() { hatch(pet); }

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
    first.stage = EvolutionStage::Baby;
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
    snap.stage = EvolutionStage::Baby;
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

std::filesystem::path makeTempSaveDir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() / "neripal-save-test" / std::to_string(stamp);
    std::filesystem::create_directories(dir);
    return dir;
}

bool fileStorageWritesViaTempAndReplace() {
    const auto dir = makeTempSaveDir();
    neripal::desktop::FileSaveStorage storage(dir);
    SaveBytes bytes{};
    bytes.size = neripal::persist::kSaveV1BlobBytes;
    bytes.data[0] = 0xAB;
    bytes.data[1] = 0xCD;
    if (storage.writeSlot(0, bytes) != StorageStatus::Ok) return false;
    if (std::filesystem::exists(dir / "neripal-slot0.bin.tmp")) return false;
    SaveBytes read{};
    if (storage.readSlot(0, read) != StorageStatus::Ok) return false;
    if (read.size != bytes.size || read.data[0] != 0xAB || read.data[1] != 0xCD) return false;
    if (storage.readSlot(1, read) != StorageStatus::Empty) return false;
    bytes.data[0] = 0xEF;
    if (storage.writeSlot(0, bytes) != StorageStatus::Ok) return false;
    if (storage.readSlot(0, read) != StorageStatus::Ok || read.data[0] != 0xEF) return false;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return true;
}

bool fileSessionSaveLoadCycle() {
    const auto dir = makeTempSaveDir();
    std::error_code ec;
    FakeClock game;
    FakeClock session;
    XorShift32 rng{1u};
    FakeWallClock wall;
    wall.set(2'000);
    neripal::desktop::FileSaveStorage storage(dir);
    Pet pet(game, rng);
    hatch(pet);
    SaveSession saves(pet, storage, wall, session);
    if (saves.boot() != BootResult::Fresh) return false;
    const int hungerBefore = pet.state().hunger;
    saves.noteCareResult(pet.feed());
    if (!saves.saveNow()) return false;
    saves.noteCareResult(pet.feed());
    if (!saves.saveNow()) return false;

    Pet again(game, rng);
    SaveSession second(again, storage, wall, session);
    const bool restored = second.boot() == BootResult::Restored && again.state().hunger < hungerBefore;

    const auto newer = storage.slotPath(1);
    {
        std::fstream file(newer, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) return false;
        file.seekp(20);
        char xorByte = 0;
        file.seekg(20);
        file.get(xorByte);
        file.seekp(20);
        file.put(static_cast<char>(static_cast<unsigned char>(xorByte) ^ 0x7Fu));
    }
    Pet third(game, rng);
    SaveSession thirdSession(third, storage, wall, session);
    const bool backup = thirdSession.boot() == BootResult::RestoredFromBackup;

    std::filesystem::remove(storage.slotPath(0), ec);
    std::filesystem::remove(storage.slotPath(1), ec);
    Pet fourth(game, rng);
    SaveSession fourthSession(fourth, storage, wall, session);
    const bool fresh = fourthSession.boot() == BootResult::Fresh;
    std::filesystem::remove_all(dir, ec);
    return restored && backup && fresh;
}

bool encodeV2Ok(const SaveRecord& record, std::uint8_t* blob, std::uint16_t& written) {
    return neripal::persist::encodeV2(record, blob, neripal::persist::kSaveBlobCapacity, written) &&
           written == neripal::persist::kSaveV2BlobBytes;
}

SaveRecord fullV2Record() {
    using neripal::core::EpisodeState;
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    SaveRecord record;
    record.sequence = 9;
    record.savedUnixSeconds = -15;
    auto& snap = record.snapshot;
    snap.hunger = 11;
    snap.happiness = 22;
    snap.energy = 33;
    snap.health = 44;
    snap.hygiene = 55;
    snap.affection = 66;
    snap.stimulation = 77;
    snap.ageMillis = 0x0102030405060708ull;
    snap.needsRemainderMs = 12'345;
    snap.needsStepPhase = 7;
    snap.stage = EvolutionStage::Adult;
    snap.form = FormId::AdultSecret;
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 4'000;
    snap.care.episodes[0].state = EpisodeState::Open;
    snap.care.episodes[0].stepsRemaining = neripal::core::balance::kAttentionWindowSteps;
    snap.care.episodes[2].state = EpisodeState::Counted;
    snap.care.lifetimeCareMistakes = 4;
    auto& baby = neripal::core::historyFor(snap.care, EvolutionStage::Baby);
    baby.careMistakes = 1;
    baby.trainCount = 2;
    baby.steps = 300;
    baby.healthGoodSteps = 10;
    baby.healthPoorSteps = 3;
    baby.happinessGoodSteps = 8;
    baby.happinessPoorSteps = 1;
    baby.responseCount = 1;
    baby.responseStepsSum = 4;
    auto& child = neripal::core::historyFor(snap.care, EvolutionStage::Child);
    child.steps = 1'080;
    child.trainCount = 18;
    child.healthGoodSteps = 900;
    child.happinessGoodSteps = 800;
    neripal::core::historyFor(snap.care, EvolutionStage::Adult).steps = 12;
    neripal::core::historyFor(snap.care, EvolutionStage::Final).steps = 1;
    snap.noticeCount = 2;
    snap.notices[0].from = EvolutionStage::Egg;
    snap.notices[0].to = EvolutionStage::Baby;
    snap.notices[0].form = FormId::Juvenile;
    snap.notices[1].from = EvolutionStage::Baby;
    snap.notices[1].to = EvolutionStage::Child;
    snap.notices[1].form = FormId::Juvenile;
    return record;
}

bool v2PayloadFitsStorage() {
    namespace P = neripal::persist;
    return P::kSaveV2PayloadBytes == 221 && P::kSaveV2BlobBytes == 233 &&
           P::kSaveV2BlobBytes <= P::kSaveBlobCapacity &&
           P::kSaveBlobCapacity - P::kSaveV2BlobBytes >= 32 &&
           2 * P::kSaveBlobCapacity < 0x5000;
}

bool codecV2Roundtrip() {
    const auto record = fullV2Record();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    if (blob[4] != 2 || blob[5] != 0) return false;
    const auto decoded = neripal::persist::decode(blob, written);
    return decoded.status == DecodeStatus::Ok && decoded.record.sequence == 9 &&
           decoded.record.savedUnixSeconds == -15 &&
           SaveSession::sameSnapshot(decoded.record.snapshot, record.snapshot);
}

bool migratedV1(EvolutionStage stage, neripal::core::FormId form) {
    SaveRecord record;
    record.sequence = 3;
    record.savedUnixSeconds = 80;
    record.snapshot.stage = stage;
    record.snapshot.hunger = 41;
    record.snapshot.ageMillis = 99;
    record.snapshot.needsRemainderMs = 1'000;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    const auto decoded = neripal::persist::decode(blob, written);
    if (decoded.status != DecodeStatus::Ok) return false;
    const auto& snap = decoded.record.snapshot;
    if (snap.stage != stage || snap.form != form) return false;
    if (snap.affection != 70 || snap.stimulation != 70 || snap.needsStepPhase != 0) return false;
    if (snap.noticeCount != 0 || snap.care.lifetimeCareMistakes != 0) return false;
    if (snap.hunger != 41 || snap.ageMillis != 99 || snap.needsRemainderMs != 1'000) return false;
    if (decoded.record.sequence != 3 || decoded.record.savedUnixSeconds != 80) return false;
    for (const auto& episode : snap.care.episodes) {
        if (episode.state != neripal::core::EpisodeState::None || episode.stepsRemaining != 0) {
            return false;
        }
    }
    const EvolutionStage stages[] = {EvolutionStage::Egg, EvolutionStage::Baby, EvolutionStage::Child,
                                     EvolutionStage::Adult, EvolutionStage::Final};
    for (const auto historyStage : stages) {
        const auto& history = neripal::core::historyFor(snap.care, historyStage);
        if (history.steps != 0 || history.careMistakes != 0 || history.trainCount != 0) return false;
    }
    return snap.notices[0].form == neripal::core::FormId::None;
}

bool codecMigratesV1Egg() { return migratedV1(EvolutionStage::Egg, neripal::core::FormId::None); }

bool codecMigratesV1Baby() {
    return migratedV1(EvolutionStage::Baby, neripal::core::FormId::Juvenile);
}

bool codecMigratesV1Child() {
    return migratedV1(EvolutionStage::Child, neripal::core::FormId::Juvenile);
}

bool codecMigratesV1Adult() {
    return migratedV1(EvolutionStage::Adult, neripal::core::FormId::AdultC);
}

bool codecMigratesV1Final() {
    return migratedV1(EvolutionStage::Final, neripal::core::FormId::AdultC);
}

bool v1BytesAreNotReadAsV2() {
    SaveRecord record;
    record.snapshot.stage = EvolutionStage::Baby;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeOk(record, blob, written)) return false;
    blob[4] = 2;
    blob[5] = 0;
    return neripal::persist::decode(blob, written).status == DecodeStatus::BadLength;
}

bool codecV2RejectsCorruptCrc() {
    const auto record = fullV2Record();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    blob[12 + neripal::persist::kSaveV2OffHistory] ^= 0x01;
    return neripal::persist::decode(blob, written).status == DecodeStatus::BadChecksum;
}

bool codecV2RejectsInvalidForm() {
    auto record = fullV2Record();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    blob[12 + neripal::persist::kSaveV2OffForm] = 99;
    rewriteCrc(blob);
    if (neripal::persist::decode(blob, written).status != DecodeStatus::InvalidForm) return false;
    blob[12 + neripal::persist::kSaveV2OffForm] =
        static_cast<std::uint8_t>(neripal::core::FormId::Juvenile);
    rewriteCrc(blob);
    return neripal::persist::decode(blob, written).status == DecodeStatus::InvalidForm;
}

bool codecV2RejectsInvalidEpisode() {
    const auto record = fullV2Record();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    blob[12 + neripal::persist::kSaveV2OffEpisodes] = 9;
    rewriteCrc(blob);
    if (neripal::persist::decode(blob, written).status != DecodeStatus::InvalidEpisode) return false;
    blob[12 + neripal::persist::kSaveV2OffEpisodes] =
        static_cast<std::uint8_t>(neripal::core::EpisodeState::Open);
    blob[12 + neripal::persist::kSaveV2OffEpisodes + 1] = 0;
    rewriteCrc(blob);
    return neripal::persist::decode(blob, written).status == DecodeStatus::InvalidEpisode;
}

bool codecV2RejectsInvalidNotice() {
    const auto record = fullV2Record();
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    blob[12 + neripal::persist::kSaveV2OffNoticeCount] = 5;
    rewriteCrc(blob);
    if (neripal::persist::decode(blob, written).status != DecodeStatus::InvalidNotice) return false;
    blob[12 + neripal::persist::kSaveV2OffNoticeCount] = 1;
    blob[12 + neripal::persist::kSaveV2OffNotices + 1] =
        static_cast<std::uint8_t>(EvolutionStage::Adult);
    rewriteCrc(blob);
    return neripal::persist::decode(blob, written).status == DecodeStatus::InvalidNotice;
}

bool sameSnapshotDetectsNewFields() {
    PetSnapshot left;
    left.stage = EvolutionStage::Baby;
    left.form = neripal::core::FormId::Juvenile;
    if (!SaveSession::sameSnapshot(left, left)) return false;
    auto right = left;
    right.affection = left.affection + 1;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.stimulation = left.stimulation + 1;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.needsStepPhase = 3;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.form = neripal::core::FormId::None;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    neripal::core::historyFor(right.care, EvolutionStage::Child).steps = 8;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.care.episodes[1].state = neripal::core::EpisodeState::Open;
    right.care.episodes[1].stepsRemaining = 4;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.care.lifetimeCareMistakes = 2;
    if (SaveSession::sameSnapshot(left, right)) return false;
    right = left;
    right.noticeCount = 1;
    right.notices[0].from = EvolutionStage::Egg;
    right.notices[0].to = EvolutionStage::Baby;
    right.notices[0].form = neripal::core::FormId::Juvenile;
    return !SaveSession::sameSnapshot(left, right);
}

bool loadAdultOrFinalDoesNotReroll(EvolutionStage stage) {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng(std::vector<std::uint32_t>{0u});
    Pet pet(clock, rng);
    SaveRecord record;
    record.snapshot.stage = stage;
    record.snapshot.form = neripal::core::FormId::AdultB;
    record.snapshot.sleepCause = SleepCause::Player;
    record.snapshot.ageMillis = stage == EvolutionStage::Final ? E::kFinalAgeMs : E::kAdultAgeMs;
    auto& child = neripal::core::historyFor(record.snapshot.care, EvolutionStage::Child);
    child.trainCount = 18;
    child.steps = 100;
    child.healthGoodSteps = 100;
    child.happinessGoodSteps = 100;
    std::uint8_t blob[neripal::persist::kSaveBlobCapacity]{};
    std::uint16_t written = 0;
    if (!encodeV2Ok(record, blob, written)) return false;
    const auto decoded = neripal::persist::decode(blob, written);
    if (decoded.status != DecodeStatus::Ok || rng.remaining() != 1) return false;
    pet.restoreSnapshot(decoded.record.snapshot);
    if (pet.state().form != neripal::core::FormId::AdultB || pet.state().stage != stage) return false;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == stage && pet.state().form == neripal::core::FormId::AdultB &&
           rng.remaining() == 1;
}

bool loadAdultDoesNotReroll() { return loadAdultOrFinalDoesNotReroll(EvolutionStage::Adult); }

bool loadFinalDoesNotReroll() { return loadAdultOrFinalDoesNotReroll(EvolutionStage::Final); }

bool noticeSurvivesSaveUntilConfirmed() {
    SessionEnv env;
    env.wall.set(5'000);
    if (env.saves.boot() != BootResult::Fresh) return false;
    auto snap = env.pet.capture();
    snap.stage = EvolutionStage::Child;
    snap.form = neripal::core::FormId::Juvenile;
    snap.noticeCount = 1;
    snap.notices[0].from = EvolutionStage::Baby;
    snap.notices[0].to = EvolutionStage::Child;
    snap.notices[0].form = neripal::core::FormId::Juvenile;
    env.pet.restoreSnapshot(snap);
    if (!env.saves.saveNow()) return false;

    FakeClock game2;
    FakeClock session2;
    XorShift32 rng2{3u};
    Pet loaded(game2, rng2);
    SaveSession second(loaded, env.store, env.wall, session2);
    if (second.boot() != BootResult::Restored) return false;
    neripal::core::EvolutionNotice notice{};
    if (loaded.pendingEvolutionNotices() != 1 || !loaded.peekEvolutionNotice(notice)) return false;
    if (notice.from != EvolutionStage::Baby || notice.to != EvolutionStage::Child ||
        notice.form != neripal::core::FormId::Juvenile) {
        return false;
    }
    if (loaded.state().stage != EvolutionStage::Child ||
        loaded.state().form != neripal::core::FormId::Juvenile) {
        return false;
    }
    if (!loaded.confirmEvolutionNotice() || !second.saveNow()) return false;

    Pet again(game2, rng2);
    SaveSession third(again, env.store, env.wall, session2);
    if (third.boot() != BootResult::Restored) return false;
    return again.pendingEvolutionNotices() == 0 && again.state().stage == EvolutionStage::Child &&
           again.state().form == neripal::core::FormId::Juvenile;
}

bool fileStorageAcceptsV2Blob() {
    const auto dir = makeTempSaveDir();
    neripal::desktop::FileSaveStorage storage(dir);
    SaveBytes bytes{};
    bytes.size = neripal::persist::kSaveV2BlobBytes;
    bytes.data[0] = 0x11;
    bytes.data[neripal::persist::kSaveV2BlobBytes - 1] = 0x5A;
    const bool wrote = storage.writeSlot(0, bytes) == StorageStatus::Ok;
    SaveBytes read{};
    const bool roundtrip = wrote && storage.readSlot(0, read) == StorageStatus::Ok &&
                           read.size == bytes.size && read.data[0] == 0x11 &&
                           read.data[read.size - 1] == 0x5A;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return roundtrip;
}

bool systemWallClockIsTrustedNow() {
    neripal::desktop::SystemWallClock wall;
    const auto now = wall.nowUnixSeconds();
    return now.has_value() && *now > 1'577'836'800;
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
        {"file storage writes via temp and replace", fileStorageWritesViaTempAndReplace},
        {"file session save load cycle", fileSessionSaveLoadCycle},
        {"system wall clock is trusted now", systemWallClockIsTrustedNow},
        {"v2 payload fits storage", v2PayloadFitsStorage},
        {"codec v2 roundtrip", codecV2Roundtrip},
        {"codec migrates v1 egg", codecMigratesV1Egg},
        {"codec migrates v1 baby", codecMigratesV1Baby},
        {"codec migrates v1 child", codecMigratesV1Child},
        {"codec migrates v1 adult", codecMigratesV1Adult},
        {"codec migrates v1 final", codecMigratesV1Final},
        {"v1 bytes are not read as v2", v1BytesAreNotReadAsV2},
        {"codec v2 rejects corrupt crc", codecV2RejectsCorruptCrc},
        {"codec v2 rejects invalid form", codecV2RejectsInvalidForm},
        {"codec v2 rejects invalid episode", codecV2RejectsInvalidEpisode},
        {"codec v2 rejects invalid notice", codecV2RejectsInvalidNotice},
        {"same snapshot detects new fields", sameSnapshotDetectsNewFields},
        {"load adult does not reroll", loadAdultDoesNotReroll},
        {"load final does not reroll", loadFinalDoesNotReroll},
        {"notice survives save until confirmed", noticeSurvivesSaveUntilConfirmed},
        {"file storage accepts v2 blob", fileStorageAcceptsV2Blob},
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
