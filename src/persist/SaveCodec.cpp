#include "neripal/persist/SaveCodec.hpp"

#include "neripal/core/Balance.hpp"
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
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
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
    return raw <= static_cast<std::uint8_t>(core::EvolutionStage::Final);
}

bool knownSleepCause(std::uint8_t raw) noexcept {
    return raw <= static_cast<std::uint8_t>(core::SleepCause::Nap);
}

bool knownForm(std::uint8_t raw) noexcept {
    return raw <= static_cast<std::uint8_t>(core::FormId::AdultSecret);
}

bool formMatchesStage(core::EvolutionStage stage, core::FormId form) noexcept {
    using core::EvolutionStage;
    using core::FormId;
    switch (stage) {
        case EvolutionStage::Egg:
            return form == FormId::None;
        case EvolutionStage::Baby:
        case EvolutionStage::Child:
            return form == FormId::Juvenile;
        case EvolutionStage::Adult:
        case EvolutionStage::Final:
            return form == FormId::AdultA || form == FormId::AdultB || form == FormId::AdultC ||
                   form == FormId::AdultSecret;
    }
    return false;
}

core::FormId migratedForm(core::EvolutionStage stage) noexcept {
    using core::EvolutionStage;
    using core::FormId;
    switch (stage) {
        case EvolutionStage::Egg:
            return FormId::None;
        case EvolutionStage::Baby:
        case EvolutionStage::Child:
            return FormId::Juvenile;
        case EvolutionStage::Adult:
        case EvolutionStage::Final:
            return FormId::AdultC;
    }
    return FormId::None;
}

bool knownEpisode(std::uint8_t stateRaw, std::uint8_t steps) noexcept {
    if (steps > core::balance::kAttentionWindowSteps) {
        return false;
    }
    const auto state = static_cast<core::EpisodeState>(stateRaw);
    if (state == core::EpisodeState::None || state == core::EpisodeState::Counted) {
        return steps == 0;
    }
    if (state == core::EpisodeState::Open) {
        return steps > 0;
    }
    return false;
}

bool adjacentTransition(core::EvolutionStage from, core::EvolutionStage to) noexcept {
    return static_cast<std::uint8_t>(to) == static_cast<std::uint8_t>(from) + 1u &&
           to <= core::EvolutionStage::Final;
}

bool knownNotice(std::uint8_t fromRaw, std::uint8_t toRaw, std::uint8_t formRaw) noexcept {
    if (!knownStage(fromRaw) || !knownStage(toRaw) || !knownForm(formRaw)) {
        return false;
    }
    const auto from = static_cast<core::EvolutionStage>(fromRaw);
    const auto to = static_cast<core::EvolutionStage>(toRaw);
    const auto form = static_cast<core::FormId>(formRaw);
    return adjacentTransition(from, to) && formMatchesStage(to, form);
}

void writeHistory(std::uint8_t* p, const core::StageHistory& history) noexcept {
    putU16LE(p + 0, history.careMistakes);
    putU16LE(p + 2, history.trainCount);
    putU32LE(p + 4, history.steps);
    putU32LE(p + 8, history.healthGoodSteps);
    putU32LE(p + 12, history.healthPoorSteps);
    putU32LE(p + 16, history.happinessGoodSteps);
    putU32LE(p + 20, history.happinessPoorSteps);
    putU16LE(p + 24, history.responseCount);
    putU32LE(p + 26, history.responseStepsSum);
}

void readHistory(const std::uint8_t* p, core::StageHistory& history) noexcept {
    history.careMistakes = getU16LE(p + 0);
    history.trainCount = getU16LE(p + 2);
    history.steps = getU32LE(p + 4);
    history.healthGoodSteps = getU32LE(p + 8);
    history.healthPoorSteps = getU32LE(p + 12);
    history.happinessGoodSteps = getU32LE(p + 16);
    history.happinessPoorSteps = getU32LE(p + 20);
    history.responseCount = getU16LE(p + 24);
    history.responseStepsSum = getU32LE(p + 26);
}

bool sameHistory(const core::StageHistory& a, const core::StageHistory& b) noexcept {
    return a.careMistakes == b.careMistakes && a.trainCount == b.trainCount && a.steps == b.steps &&
           a.healthGoodSteps == b.healthGoodSteps && a.healthPoorSteps == b.healthPoorSteps &&
           a.happinessGoodSteps == b.happinessGoodSteps &&
           a.happinessPoorSteps == b.happinessPoorSteps && a.responseCount == b.responseCount &&
           a.responseStepsSum == b.responseStepsSum;
}

bool writeBlob(std::uint8_t* out, std::size_t capacity, std::uint16_t version,
               const std::uint8_t* payload, std::uint16_t payloadBytes,
               std::uint16_t& written) noexcept {
    const auto blobBytes = static_cast<std::uint16_t>(kSaveHeaderBytes + payloadBytes);
    if (out == nullptr || payload == nullptr || capacity < blobBytes) {
        return false;
    }
    putU32LE(out + 0, kSaveMagic);
    putU16LE(out + 4, version);
    putU16LE(out + 6, payloadBytes);
    putU32LE(out + 8, crc32(payload, payloadBytes));
    for (std::uint16_t i = 0; i < payloadBytes; ++i) {
        out[kSaveHeaderBytes + i] = payload[i];
    }
    written = blobBytes;
    return true;
}

void migrateV1(core::PetSnapshot& snapshot) noexcept {
    snapshot.affection = core::balance::kAffectionStart;
    snapshot.stimulation = core::balance::kStimulationStart;
    snapshot.needsStepPhase = 0;
    snapshot.form = migratedForm(snapshot.stage);
    snapshot.care = {};
    snapshot.noticeCount = 0;
    for (auto& notice : snapshot.notices) {
        notice = {};
    }
}

DecodeResult fail(DecodeStatus status) noexcept {
    DecodeResult result;
    result.status = status;
    return result;
}

struct CheckedHeader {
    DecodeStatus status = DecodeStatus::Truncated;
    std::uint16_t version = 0;
    std::uint16_t payloadBytes = 0;
    const std::uint8_t* payload = nullptr;
};

CheckedHeader checkHeader(const std::uint8_t* data, std::size_t size, std::uint16_t version,
                          std::uint16_t expectedPayload) noexcept {
    CheckedHeader header;
    if (data == nullptr || size < kSaveHeaderBytes) {
        header.status = DecodeStatus::Truncated;
        return header;
    }
    if (getU32LE(data + 0) != kSaveMagic) {
        header.status = DecodeStatus::BadMagic;
        return header;
    }
    header.version = getU16LE(data + 4);
    if (header.version != version) {
        header.status = DecodeStatus::UnsupportedVersion;
        return header;
    }
    header.payloadBytes = getU16LE(data + 6);
    if (header.payloadBytes != expectedPayload) {
        header.status = DecodeStatus::BadLength;
        return header;
    }
    if (size < static_cast<std::size_t>(kSaveHeaderBytes + header.payloadBytes)) {
        header.status = DecodeStatus::Truncated;
        return header;
    }
    header.payload = data + kSaveHeaderBytes;
    if (crc32(header.payload, header.payloadBytes) != getU32LE(data + 8)) {
        header.status = DecodeStatus::BadChecksum;
        return header;
    }
    header.status = DecodeStatus::Ok;
    return header;
}

DecodeResult decodeV1(const std::uint8_t* data, std::size_t size) noexcept {
    const auto header = checkHeader(data, size, kSaveVersionV1, kSaveV1PayloadBytes);
    if (header.status != DecodeStatus::Ok) {
        return fail(header.status);
    }
    const std::uint8_t* payload = header.payload;
    const auto stageRaw = getU8(payload + 34);
    if (!knownStage(stageRaw)) {
        return fail(DecodeStatus::InvalidStage);
    }
    const auto sleepRaw = getU8(payload + 35);
    if (!knownSleepCause(sleepRaw)) {
        return fail(DecodeStatus::InvalidSleepCause);
    }

    DecodeResult result;
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
    migrateV1(result.record.snapshot);
    result.status = DecodeStatus::Ok;
    return result;
}

DecodeResult decodeV2(const std::uint8_t* data, std::size_t size) noexcept {
    const auto header = checkHeader(data, size, kSaveVersionV2, kSaveV2PayloadBytes);
    if (header.status != DecodeStatus::Ok) {
        return fail(header.status);
    }
    const std::uint8_t* payload = header.payload;

    const auto phase = getU8(payload + kSaveV2OffPhase);
    if (phase >= core::balance::kNeedsPhaseCycle) {
        return fail(DecodeStatus::InvalidNeedsPhase);
    }
    const auto stageRaw = getU8(payload + kSaveV2OffStage);
    if (!knownStage(stageRaw)) {
        return fail(DecodeStatus::InvalidStage);
    }
    const auto formRaw = getU8(payload + kSaveV2OffForm);
    if (!knownForm(formRaw) ||
        !formMatchesStage(static_cast<core::EvolutionStage>(stageRaw),
                          static_cast<core::FormId>(formRaw))) {
        return fail(DecodeStatus::InvalidForm);
    }
    const auto sleepRaw = getU8(payload + kSaveV2OffSleep);
    if (!knownSleepCause(sleepRaw)) {
        return fail(DecodeStatus::InvalidSleepCause);
    }

    for (std::uint8_t index = 0; index < core::kBaseNeedCount; ++index) {
        const auto* episode = payload + kSaveV2OffEpisodes + index * kSaveV2EpisodeBytes;
        if (!knownEpisode(episode[0], episode[1])) {
            return fail(DecodeStatus::InvalidEpisode);
        }
    }

    const auto noticeCount = getU8(payload + kSaveV2OffNoticeCount);
    if (noticeCount > core::kEvolutionNoticeCapacity) {
        return fail(DecodeStatus::InvalidNotice);
    }
    for (std::uint8_t index = 0; index < core::kEvolutionNoticeCapacity; ++index) {
        const auto* notice = payload + kSaveV2OffNotices + index * kSaveV2NoticeBytes;
        if (index < noticeCount) {
            if (!knownNotice(notice[0], notice[1], notice[2])) {
                return fail(DecodeStatus::InvalidNotice);
            }
        } else if (notice[0] != 0 || notice[1] != 0 || notice[2] != 0) {
            return fail(DecodeStatus::InvalidNotice);
        }
    }

    DecodeResult result;
    auto& snapshot = result.record.snapshot;
    result.record.sequence = getU32LE(payload + kSaveV2OffSequence);
    result.record.savedUnixSeconds = getI64LE(payload + kSaveV2OffUnix);
    snapshot.ageMillis = getU64LE(payload + kSaveV2OffAge);
    snapshot.needsRemainderMs = getU32LE(payload + kSaveV2OffRemainder);
    snapshot.hunger = getI16LE(payload + kSaveV2OffHunger);
    snapshot.happiness = getI16LE(payload + kSaveV2OffHunger + 2);
    snapshot.energy = getI16LE(payload + kSaveV2OffHunger + 4);
    snapshot.health = getI16LE(payload + kSaveV2OffHunger + 6);
    snapshot.hygiene = getI16LE(payload + kSaveV2OffHunger + 8);
    snapshot.affection = getI16LE(payload + kSaveV2OffAffection);
    snapshot.stimulation = getI16LE(payload + kSaveV2OffAffection + 2);
    snapshot.needsStepPhase = phase;
    snapshot.stage = static_cast<core::EvolutionStage>(stageRaw);
    snapshot.form = static_cast<core::FormId>(formRaw);
    snapshot.sleepCause = static_cast<core::SleepCause>(sleepRaw);
    snapshot.napRemainingMs = getU32LE(payload + kSaveV2OffNap);
    for (std::uint8_t index = 0; index < core::kBaseNeedCount; ++index) {
        const auto* episode = payload + kSaveV2OffEpisodes + index * kSaveV2EpisodeBytes;
        snapshot.care.episodes[index].state = static_cast<core::EpisodeState>(episode[0]);
        snapshot.care.episodes[index].stepsRemaining = episode[1];
    }
    snapshot.care.lifetimeCareMistakes = getU16LE(payload + kSaveV2OffLifetime);
    for (std::uint8_t index = 0; index < kPersistedStageCount; ++index) {
        readHistory(payload + kSaveV2OffHistory + index * kSaveV2StageHistoryBytes,
                    snapshot.care.stages[index]);
    }
    snapshot.noticeCount = noticeCount;
    for (std::uint8_t index = 0; index < noticeCount; ++index) {
        const auto* notice = payload + kSaveV2OffNotices + index * kSaveV2NoticeBytes;
        snapshot.notices[index].from = static_cast<core::EvolutionStage>(notice[0]);
        snapshot.notices[index].to = static_cast<core::EvolutionStage>(notice[1]);
        snapshot.notices[index].form = static_cast<core::FormId>(notice[2]);
    }
    result.status = DecodeStatus::Ok;
    return result;
}

}  // namespace

bool encodeV1(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept {
    written = 0;
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
    return writeBlob(out, capacity, kSaveVersionV1, payload, kSaveV1PayloadBytes, written);
}

bool encodeV2(const SaveRecord& record, std::uint8_t* out, std::size_t capacity,
              std::uint16_t& written) noexcept {
    written = 0;
    std::uint8_t payload[kSaveV2PayloadBytes]{};
    const auto& snapshot = record.snapshot;
    putU32LE(payload + kSaveV2OffSequence, record.sequence);
    putI64LE(payload + kSaveV2OffUnix, record.savedUnixSeconds);
    putU64LE(payload + kSaveV2OffAge, snapshot.ageMillis);
    putU32LE(payload + kSaveV2OffRemainder, snapshot.needsRemainderMs);
    putI16LE(payload + kSaveV2OffHunger, clampToI16(snapshot.hunger));
    putI16LE(payload + kSaveV2OffHunger + 2, clampToI16(snapshot.happiness));
    putI16LE(payload + kSaveV2OffHunger + 4, clampToI16(snapshot.energy));
    putI16LE(payload + kSaveV2OffHunger + 6, clampToI16(snapshot.health));
    putI16LE(payload + kSaveV2OffHunger + 8, clampToI16(snapshot.hygiene));
    putI16LE(payload + kSaveV2OffAffection, clampToI16(snapshot.affection));
    putI16LE(payload + kSaveV2OffAffection + 2, clampToI16(snapshot.stimulation));
    putU8(payload + kSaveV2OffPhase, snapshot.needsStepPhase);
    putU8(payload + kSaveV2OffStage, static_cast<std::uint8_t>(snapshot.stage));
    putU8(payload + kSaveV2OffForm, static_cast<std::uint8_t>(snapshot.form));
    putU8(payload + kSaveV2OffSleep, static_cast<std::uint8_t>(snapshot.sleepCause));
    putU32LE(payload + kSaveV2OffNap, snapshot.napRemainingMs);
    for (std::uint8_t index = 0; index < core::kBaseNeedCount; ++index) {
        auto* episode = payload + kSaveV2OffEpisodes + index * kSaveV2EpisodeBytes;
        episode[0] = static_cast<std::uint8_t>(snapshot.care.episodes[index].state);
        episode[1] = snapshot.care.episodes[index].stepsRemaining;
    }
    putU16LE(payload + kSaveV2OffLifetime, snapshot.care.lifetimeCareMistakes);
    for (std::uint8_t index = 0; index < kPersistedStageCount; ++index) {
        writeHistory(payload + kSaveV2OffHistory + index * kSaveV2StageHistoryBytes,
                     snapshot.care.stages[index]);
    }
    const auto noticeCount = snapshot.noticeCount <= core::kEvolutionNoticeCapacity
                                 ? snapshot.noticeCount
                                 : core::kEvolutionNoticeCapacity;
    putU8(payload + kSaveV2OffNoticeCount, noticeCount);
    for (std::uint8_t index = 0; index < noticeCount; ++index) {
        auto* notice = payload + kSaveV2OffNotices + index * kSaveV2NoticeBytes;
        notice[0] = static_cast<std::uint8_t>(snapshot.notices[index].from);
        notice[1] = static_cast<std::uint8_t>(snapshot.notices[index].to);
        notice[2] = static_cast<std::uint8_t>(snapshot.notices[index].form);
    }
    return writeBlob(out, capacity, kSaveVersionV2, payload, kSaveV2PayloadBytes, written);
}

DecodeResult decode(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kSaveHeaderBytes) {
        return fail(DecodeStatus::Truncated);
    }
    if (getU32LE(data + 0) != kSaveMagic) {
        return fail(DecodeStatus::BadMagic);
    }
    const auto version = getU16LE(data + 4);
    if (version == kSaveVersionV1) {
        return decodeV1(data, size);
    }
    if (version == kSaveVersionV2) {
        return decodeV2(data, size);
    }
    return fail(DecodeStatus::UnsupportedVersion);
}

bool snapshotsEqual(const core::PetSnapshot& a, const core::PetSnapshot& b) noexcept {
    if (a.hunger != b.hunger || a.happiness != b.happiness || a.energy != b.energy ||
        a.health != b.health || a.hygiene != b.hygiene || a.affection != b.affection ||
        a.stimulation != b.stimulation || a.ageMillis != b.ageMillis ||
        a.needsRemainderMs != b.needsRemainderMs || a.needsStepPhase != b.needsStepPhase ||
        a.stage != b.stage || a.form != b.form || a.sleepCause != b.sleepCause ||
        a.napRemainingMs != b.napRemainingMs) {
        return false;
    }
    if (a.care.lifetimeCareMistakes != b.care.lifetimeCareMistakes) {
        return false;
    }
    for (std::size_t index = 0; index < core::kBaseNeedCount; ++index) {
        if (a.care.episodes[index].state != b.care.episodes[index].state ||
            a.care.episodes[index].stepsRemaining != b.care.episodes[index].stepsRemaining) {
            return false;
        }
    }
    for (std::size_t index = 0; index < core::kStageHistoryCapacity; ++index) {
        if (!sameHistory(a.care.stages[index], b.care.stages[index])) {
            return false;
        }
    }
    if (a.noticeCount != b.noticeCount) {
        return false;
    }
    for (std::uint8_t index = 0; index < core::kEvolutionNoticeCapacity; ++index) {
        if (a.notices[index].from != b.notices[index].from ||
            a.notices[index].to != b.notices[index].to ||
            a.notices[index].form != b.notices[index].form) {
            return false;
        }
    }
    return true;
}

}  // namespace neripal::persist
