#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/persist/Crc32.hpp"
#include "neripal/persist/SaveCodec.hpp"

#include "FakeClock.hpp"
#include "FakeRandom.hpp"

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
using neripal::persist::DecodeStatus;
using neripal::persist::SaveRecord;

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
