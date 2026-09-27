#pragma once

#include <cstdint>

namespace neripal::core::balance {

inline constexpr int kMinStat = 0;
inline constexpr int kMaxStat = 100;
inline constexpr std::uint64_t kNeedsStepMs = 60'000;

inline constexpr int kFeedHunger = -25;
inline constexpr int kFeedHappiness = 3;
inline constexpr int kFeedHygiene = -2;
inline constexpr int kTrainEnergy = -15;
inline constexpr int kTrainHunger = 8;
inline constexpr int kTrainHappiness = 8;
inline constexpr int kTrainHealth = 2;
inline constexpr int kTrainHygiene = -4;
inline constexpr int kCleanHygiene = 40;
inline constexpr int kCleanHappiness = 5;
inline constexpr int kTrainStimulation = 25;
inline constexpr int kPetAffection = 25;
inline constexpr int kPetHappiness = 3;
inline constexpr int kPlayStimulation = 25;
inline constexpr int kPlayEnergy = -10;
inline constexpr int kPlayHunger = 5;
inline constexpr int kPlayHappiness = 5;

// Provisional 0.6 starting values. Normal for both stats is above 40.
inline constexpr int kAffectionStart = 70;
inline constexpr int kStimulationStart = 70;

// Provisional cadences. One point per this many minute-steps.
inline constexpr int kHygieneSleepInterval = 4;
inline constexpr int kAffectionAwakeInterval = 3;
inline constexpr int kAffectionSleepInterval = 6;
inline constexpr int kAffectionPerHit = -1;
inline constexpr int kStimulationAwakePerStep = -1;
// Least common multiple of the intervals above. needsStepPhase cycles through it.
inline constexpr std::uint8_t kNeedsPhaseCycle = 12;

// Provisional need levels. Hunger rises toward Urgent; the other base needs fall.
inline constexpr int kHungerAttention = 60;
inline constexpr int kHungerUrgent = 80;
inline constexpr int kLowNeedAttention = 40;
inline constexpr int kLowNeedUrgent = 20;

// Provisional care-history thresholds. Not approved evolution content.
inline constexpr std::uint8_t kAttentionWindowSteps = 15;
inline constexpr std::uint16_t kTrainingModerateCount = 6;
inline constexpr std::uint16_t kTrainingFrequentCount = 18;
inline constexpr int kHistoryGoodStat = 70;
inline constexpr int kHistoryPoorStat = 40;

// Fixture gates for the 0.6 rule table. Not approved evolution content.
inline constexpr std::uint8_t kFixtureSpecialGoodPercent = 90;
inline constexpr std::uint8_t kFixtureHappinessBranchPercent = 60;
inline constexpr std::uint16_t kFixtureHappinessMaxMistakes = 2;

inline constexpr int kHungerPerStep = 1;
inline constexpr int kAwakeEnergyPerStep = -1;
inline constexpr int kSleepEnergyPerStep = 2;
inline constexpr int kHygienePerAwakeStep = -1;
inline constexpr int kHygieneNeglectThreshold = 30;
inline constexpr int kHappinessUrgentPerStep = -1;
inline constexpr int kCriticalHealthPerStep = -1;

inline constexpr int kTiredMoodEnergy = 20;
inline constexpr int kAnnoyedMoodHappiness = 35;
inline constexpr int kAnnoyedMoodHunger = 75;
inline constexpr int kAnnoyedMoodHealth = 30;
inline constexpr int kHappyMoodHappiness = 80;

inline constexpr std::uint32_t kMaxActivityTransitionsPerUpdate = 8;
inline constexpr std::uint32_t kIdleDurationMinMs = 2500;
inline constexpr std::uint32_t kIdleDurationMaxMs = 5000;
inline constexpr std::uint32_t kIdleTiredDurationBonusMs = 1500;
inline constexpr int kIdleHighEnergyThreshold = 90;
inline constexpr int kPetHomeX = 120;
inline constexpr int kDefaultFacing = 1;

inline constexpr int kWalkMinX = 40;
inline constexpr int kWalkMaxX = 176;  // 16×4 px sprite stays inside 240
inline constexpr int kWalkMinDistancePx = 24;
inline constexpr int kWalkMaxDistancePx = 64;
inline constexpr std::uint32_t kWalkMsPerPixel = 25;  // 40 px/s; 25 ms remainder is exact
inline constexpr int kWanderChancePercent = 20;
inline constexpr int kWanderAnnoyedBonusPercent = 15;
inline constexpr int kWanderHighEnergyBonusPercent = 15;
inline constexpr int kWanderTiredPenaltyPercent = 10;

inline constexpr std::uint32_t kEatDurationMs = 1400;
inline constexpr std::uint32_t kHappyDurationMs = 1000;
inline constexpr std::uint32_t kAnnoyedDurationMs = 1000;
inline constexpr std::uint32_t kTiredDurationMs = 1000;
inline constexpr std::uint32_t kDirtyDurationMs = 1000;

inline constexpr int kAutonomousNapEnergy = 15;
inline constexpr int kNapWakeEnergy = 40;
inline constexpr std::uint32_t kNapDurationMinMs = 8000;
inline constexpr std::uint32_t kNapDurationMaxMs = 20000;

// Defensive sanity cap for a bad wall clock. Not a gameplay rule: 0.6 may
// change it without touching needs catch-up.
inline constexpr std::uint64_t kMaxAgeOfflineMs = 365ull * 24 * 60 * 60 * 1000;
// Needs hit a fixed point within a few hundred minutes. The cap only bounds
// how much of a gap is fed into that catch-up.
inline constexpr std::uint64_t kMaxNeedsOfflineMs = 30ull * 24 * 60 * 60 * 1000;
// Former offline early-exit. Catch-up no longer stops at this count: a full
// needs cycle that leaves stats and stage unchanged may be applied in bulk,
// and that bulk still stops at the next stage gate. Nap is never bulked.
inline constexpr std::uint32_t kNeedsSettleSteps = 400;

}  // namespace neripal::core::balance

