#pragma once

#include "neripal/core/PetSnapshot.hpp"

#include <cstddef>
#include <cstdint>

namespace neripal::persist {

inline constexpr std::uint32_t kSaveMagic = 0x4E503031u;  // 'NP01'
inline constexpr std::uint16_t kSaveHeaderBytes = 12;
inline constexpr std::uint16_t kSaveVersionV1 = 1;
inline constexpr std::uint16_t kSaveV1PayloadBytes = 44;
inline constexpr std::uint16_t kSaveV1BlobBytes = kSaveHeaderBytes + kSaveV1PayloadBytes;
inline constexpr std::uint16_t kSaveVersionV2 = 2;

// One StageHistory, field by field, with no compiler padding.
inline constexpr std::uint16_t kSaveV2StageHistoryBytes = 30;
// Egg through Final. Spare CareRecord slots are not stages and are not stored.
inline constexpr std::uint16_t kPersistedStageCount = 5;
inline constexpr std::uint16_t kSaveV2EpisodeBytes = 2;
inline constexpr std::uint16_t kSaveV2NoticeBytes = 3;

// sequence, unix, age, remainder, seven stats, phase/stage/form/sleep, nap,
// five episodes, lifetime, five stage histories, notice count, four notices.
inline constexpr std::uint16_t kSaveV2PayloadBytes =
    4u + 8u + 8u + 4u + (2u * 7u) + 4u + 4u + (kSaveV2EpisodeBytes * 5u) + 2u +
    (kSaveV2StageHistoryBytes * kPersistedStageCount) + 1u + (kSaveV2NoticeBytes * 4u);
inline constexpr std::uint16_t kSaveV2BlobBytes = kSaveHeaderBytes + kSaveV2PayloadBytes;

// 192 does not fit the 233-byte V2 blob. 320 leaves 87 bytes of slot margin.
inline constexpr std::size_t kSaveBlobCapacity = 320;

static_assert(kSaveV1BlobBytes == 56);
static_assert(kSaveV2PayloadBytes == 221);
static_assert(kSaveV2BlobBytes == 233);
static_assert(kSaveV2BlobBytes + 32 <= kSaveBlobCapacity);
static_assert(kPersistedStageCount ==
              static_cast<std::uint16_t>(core::EvolutionStage::Final) + 1u);
static_assert(core::kStageHistoryCapacity >= kPersistedStageCount);

// V2 payload offsets. The 12-byte header (magic, version, length, crc) sits in front.
inline constexpr std::uint16_t kSaveV2OffSequence = 0;
inline constexpr std::uint16_t kSaveV2OffUnix = 4;
inline constexpr std::uint16_t kSaveV2OffAge = 12;
inline constexpr std::uint16_t kSaveV2OffRemainder = 20;
inline constexpr std::uint16_t kSaveV2OffHunger = 24;
inline constexpr std::uint16_t kSaveV2OffAffection = 34;
inline constexpr std::uint16_t kSaveV2OffPhase = 38;
inline constexpr std::uint16_t kSaveV2OffStage = 39;
inline constexpr std::uint16_t kSaveV2OffForm = 40;
inline constexpr std::uint16_t kSaveV2OffSleep = 41;
inline constexpr std::uint16_t kSaveV2OffNap = 42;
inline constexpr std::uint16_t kSaveV2OffEpisodes = 46;
inline constexpr std::uint16_t kSaveV2OffLifetime = 56;
inline constexpr std::uint16_t kSaveV2OffHistory = 58;
inline constexpr std::uint16_t kSaveV2OffNoticeCount = 208;
inline constexpr std::uint16_t kSaveV2OffNotices = 209;

static_assert(kSaveV2OffNotices + (kSaveV2NoticeBytes * core::kEvolutionNoticeCapacity) ==
              kSaveV2PayloadBytes);

enum class DecodeStatus : std::uint8_t {
    Ok = 0,
    Truncated,
    BadMagic,
    UnsupportedVersion,
    BadLength,
    BadChecksum,
    InvalidStage,
    InvalidSleepCause,
    InvalidForm,
    InvalidEpisode,
    InvalidNotice,
    InvalidNeedsPhase,
};

struct SaveRecord {
    std::uint32_t sequence = 1;
    std::int64_t savedUnixSeconds = 0;
    core::PetSnapshot snapshot{};
};

struct DecodeResult {
    DecodeStatus status = DecodeStatus::Truncated;
    SaveRecord record{};
};

bool encodeV1(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept;
bool encodeV2(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept;
DecodeResult decode(const std::uint8_t* data, std::size_t size) noexcept;

bool snapshotsEqual(const core::PetSnapshot& a, const core::PetSnapshot& b) noexcept;

}  // namespace neripal::persist
