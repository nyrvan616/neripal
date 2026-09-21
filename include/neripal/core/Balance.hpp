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

inline constexpr int kHungerPerStep = 1;
inline constexpr int kAwakeEnergyPerStep = -1;
inline constexpr int kSleepEnergyPerStep = 2;
inline constexpr int kHygienePerAwakeStep = -1;
inline constexpr int kNeglectThreshold = 70;
inline constexpr int kHygieneNeglectThreshold = 30;
inline constexpr int kHappinessNeglectPerStep = -1;
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

}  // namespace neripal::core::balance

