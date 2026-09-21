#include "neripal/persist/SaveCodec.hpp"

#include "neripal/persist/Crc32.hpp"

#include <limits>

namespace neripal::persist {
namespace {

void putU8(std::uint8_t* p, std::uint8_t v) noexcept {
    p[0] = v;
}

void putU16LE(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}

void putU32LE(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v >> 16);
    p[3] = static_cast<std::uint8_t>(v >> 24);
}

void putU64LE(std::uint8_t* p, std::uint64_t v) noexcept {
    putU32LE(p, static_cast<std::uint32_t>(v));
    putU32LE(p + 4, static_cast<std::uint32_t>(v >> 32));
}

void putI16LE(std::uint8_t* p, std::int16_t v) noexcept {
    putU16LE(p, static_cast<std::uint16_t>(v));
}

void putI64LE(std::uint8_t* p, std::int64_t v) noexcept {
    putU64LE(p, static_cast<std::uint64_t>(v));
}

std::uint8_t getU8(const std::uint8_t* p) noexcept {
    return p[0];
}

std::uint16_t getU16LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t getU32LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t getU64LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint64_t>(getU32LE(p)) |
           (static_cast<std::uint64_t>(getU32LE(p + 4)) << 32);
}

std::int16_t getI16LE(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(getU16LE(p));
}

std::int64_t getI64LE(const std::uint8_t* p) noexcept {
    return static_cast<std::int64_t>(getU64LE(p));
}

std::int16_t clampToI16(int value) noexcept {
    if (value < std::numeric_limits<std::int16_t>::min()) {
        return std::numeric_limits<std::int16_t>::min();
    }
    if (value > std::numeric_limits<std::int16_t>::max()) {
        return std::numeric_limits<std::int16_t>::max();
    }
    return static_cast<std::int16_t>(value);
}

bool knownStage(std::uint8_t raw) noexcept {
    return raw <= static_cast<std::uint8_t>(core::EvolutionStage::Adult);
}

bool knownSleepCause(std::uint8_t raw) noexcept {
    return raw <= static_cast<std::uint8_t>(core::SleepCause::Nap);
}

}  // namespace

bool encodeV1(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept {
    written = 0;
    if (out == nullptr || capacity < kSaveV1BlobBytes) {
        return false;
    }

    std::uint8_t payload[kSaveV1PayloadBytes]{};
    putU32LE(payload + 0, record.sequence);
    putI64LE(payload + 4, record.savedUnixSeconds);
    putU64LE(payload + 12, record.snapshot.ageMillis);
    putU32LE(payload + 20, record.snapshot.needsRemainderMs);
    putI16LE(payload + 24, clampToI16(record.snapshot.hunger));
    putI16LE(payload + 26, clampToI16(record.snapshot.happiness));
    putI16LE(payload + 28, clampToI16(record.snapshot.energy));
    putI16LE(payload + 30, clampToI16(record.snapshot.health));
    putI16LE(payload + 32, clampToI16(record.snapshot.hygiene));
    putU8(payload + 34, static_cast<std::uint8_t>(record.snapshot.stage));
    putU8(payload + 35, static_cast<std::uint8_t>(record.snapshot.sleepCause));
    putU32LE(payload + 36, record.snapshot.napRemainingMs);
    payload[40] = 0;
    payload[41] = 0;
    payload[42] = 0;
    payload[43] = 0;

    putU32LE(out + 0, kSaveMagic);
    putU16LE(out + 4, kSaveVersionV1);
    putU16LE(out + 6, kSaveV1PayloadBytes);
    putU32LE(out + 8, crc32(payload, kSaveV1PayloadBytes));
    for (std::uint16_t i = 0; i < kSaveV1PayloadBytes; ++i) {
        out[12 + i] = payload[i];
    }
    written = kSaveV1BlobBytes;
    return true;
}

DecodeResult decode(const std::uint8_t* data, std::size_t size) noexcept {
    DecodeResult result;
    if (data == nullptr || size < 12) {
        result.status = DecodeStatus::Truncated;
        return result;
    }

    const auto magic = getU32LE(data + 0);
    if (magic != kSaveMagic) {
        result.status = DecodeStatus::BadMagic;
        return result;
    }

    const auto version = getU16LE(data + 4);
    if (version != kSaveVersionV1) {
        result.status = DecodeStatus::UnsupportedVersion;
        return result;
    }

    const auto payloadBytes = getU16LE(data + 6);
    if (payloadBytes != kSaveV1PayloadBytes) {
        result.status = DecodeStatus::BadLength;
        return result;
    }
    if (size < static_cast<std::size_t>(12 + payloadBytes)) {
        result.status = DecodeStatus::Truncated;
        return result;
    }

    const auto expectedCrc = getU32LE(data + 8);
    const std::uint8_t* payload = data + 12;
    if (crc32(payload, payloadBytes) != expectedCrc) {
        result.status = DecodeStatus::BadChecksum;
        return result;
    }

    const auto stageRaw = getU8(payload + 34);
    if (!knownStage(stageRaw)) {
        result.status = DecodeStatus::InvalidStage;
        return result;
    }
    const auto sleepRaw = getU8(payload + 35);
    if (!knownSleepCause(sleepRaw)) {
        result.status = DecodeStatus::InvalidSleepCause;
        return result;
    }

    result.record.sequence = getU32LE(payload + 0);
    result.record.savedUnixSeconds = getI64LE(payload + 4);
    result.record.snapshot.ageMillis = getU64LE(payload + 12);
    result.record.snapshot.needsRemainderMs = getU32LE(payload + 20);
    result.record.snapshot.hunger = getI16LE(payload + 24);
    result.record.snapshot.happiness = getI16LE(payload + 26);
    result.record.snapshot.energy = getI16LE(payload + 28);
    result.record.snapshot.health = getI16LE(payload + 30);
    result.record.snapshot.hygiene = getI16LE(payload + 32);
    result.record.snapshot.stage = static_cast<core::EvolutionStage>(stageRaw);
    result.record.snapshot.sleepCause = static_cast<core::SleepCause>(sleepRaw);
    result.record.snapshot.napRemainingMs = getU32LE(payload + 36);
    result.status = DecodeStatus::Ok;
    return result;
}

}  // namespace neripal::persist
