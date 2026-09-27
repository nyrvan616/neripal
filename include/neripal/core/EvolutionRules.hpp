#pragma once

#include "neripal/core/CareHistory.hpp"
#include "neripal/core/Evolution.hpp"
#include "neripal/core/IRandom.hpp"

#include <cstdint>

namespace neripal::core {

// What a rule may read. Fixtures can ignore stages; the context still exposes
// every stage slot and the lifetime total.
struct EvolutionContext {
    const CareRecord* care = nullptr;
    EvolutionStage leaving = EvolutionStage::Child;

    const StageHistory& stageHistory(EvolutionStage stage) const noexcept;
    std::uint16_t lifetimeCareMistakes() const noexcept;
};

// One optional check. Disabled gates are not consulted.
struct MistakeGate {
    bool enabled = false;
    bool lifetime = false;
    EvolutionStage stage = EvolutionStage::Child;
    std::uint16_t maximum = 0;
    bool exact = false;
};

struct TrainingGate {
    bool enabled = false;
    EvolutionStage stage = EvolutionStage::Child;
    TrainingLevel level = TrainingLevel::Low;
};

struct RatioGate {
    bool enabled = false;
    EvolutionStage stage = EvolutionStage::Child;
    std::uint8_t minPercent = 0;
};

// Declarative rule. `from` is the stage being left. One or two exits.
// Two exits are equivalent; the single RNG sample picks between them.
// Rules are not a quality ranking.
struct EvolutionRule {
    EvolutionStage from = EvolutionStage::Child;
    FormId exits[2] = {FormId::None, FormId::None};
    std::uint8_t exitCount = 1;
    MistakeGate mistakes{};
    TrainingGate training{};
    RatioGate healthGood{};
    RatioGate happinessGood{};
};

bool ruleMatches(const EvolutionRule& rule, const EvolutionContext& context) noexcept;

// First matching rule for `context.leaving` wins. RNG is consumed only when
// that rule has more than one exit. No match returns FormId::None.
FormId resolveEvolution(const EvolutionContext& context, IRandom& random);

}  // namespace neripal::core
