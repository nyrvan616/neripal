#pragma once

#include <cstdint>

namespace neripal::core::balance {

inline constexpr int kMinStat = 0;
inline constexpr int kMaxStat = 100;
inline constexpr std::uint64_t kNeedsStepMs = 60'000;

inline constexpr int kFeedHunger = -25;
inline constexpr int kFeedHappiness = 3;
inline constexpr int kTrainEnergy = -15;
inline constexpr int kTrainHunger = 8;
inline constexpr int kTrainHappiness = 8;
inline constexpr int kTrainHealth = 2;

inline constexpr int kHungerPerStep = 1;
inline constexpr int kAwakeEnergyPerStep = -1;
inline constexpr int kSleepEnergyPerStep = 2;
inline constexpr int kNeglectThreshold = 70;
inline constexpr int kHappinessNeglectPerStep = -1;
inline constexpr int kCriticalHealthPerStep = -1;

}  // namespace neripal::core::balance
