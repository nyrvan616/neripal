#include "neripal/core/EvolutionRules.hpp"

#include "neripal/core/Balance.hpp"

namespace neripal::core {
namespace {

bool ratioAtLeast(std::uint32_t part, std::uint32_t total, std::uint8_t percent) noexcept {
    if (total == 0) {
        return false;
    }
    return static_cast<std::uint64_t>(part) * 100ull >=
           static_cast<std::uint64_t>(total) * percent;
}

bool mistakesMatch(const MistakeGate& gate, const EvolutionContext& context) noexcept {
    if (!gate.enabled) {
        return true;
    }
    const auto count = gate.lifetime ? context.lifetimeCareMistakes()
                                     : context.stageHistory(gate.stage).careMistakes;
    if (gate.exact) {
        return count == gate.maximum;
    }
    return count <= gate.maximum;
}

bool trainingMatches(const TrainingGate& gate, const EvolutionContext& context) noexcept {
    if (!gate.enabled) {
        return true;
    }
    return trainingLevel(context.stageHistory(gate.stage)) == gate.level;
}

bool ratioMatches(const RatioGate& gate, const EvolutionContext& context,
                  bool health) noexcept {
    if (!gate.enabled) {
        return true;
    }
    const auto& history = context.stageHistory(gate.stage);
    const auto part = health ? history.healthGoodSteps : history.happinessGoodSteps;
    return ratioAtLeast(part, history.steps, gate.minPercent);
}

// Fixture table for Child -> Adult. Not approved creature content.
// Order is the priority: the first match wins. There is no tie-break roll.
constexpr EvolutionRule specialRule() {
    EvolutionRule rule{};
    rule.from = EvolutionStage::Child;
    rule.exits[0] = FormId::AdultSecret;
    rule.exits[1] = FormId::AdultB;
    rule.exitCount = 2;
    rule.mistakes.enabled = true;
    rule.mistakes.stage = EvolutionStage::Child;
    rule.mistakes.maximum = 0;
    rule.mistakes.exact = true;
    rule.training.enabled = true;
    rule.training.stage = EvolutionStage::Child;
    rule.training.level = TrainingLevel::Moderate;
    rule.healthGood.enabled = true;
    rule.healthGood.stage = EvolutionStage::Child;
    rule.healthGood.minPercent = balance::kFixtureSpecialGoodPercent;
    rule.happinessGood.enabled = true;
    rule.happinessGood.stage = EvolutionStage::Child;
    rule.happinessGood.minPercent = balance::kFixtureSpecialGoodPercent;
    return rule;
}

constexpr EvolutionRule frequentRule() {
    EvolutionRule rule{};
    rule.from = EvolutionStage::Child;
    rule.exits[0] = FormId::AdultA;
    rule.exitCount = 1;
    rule.training.enabled = true;
    rule.training.stage = EvolutionStage::Child;
    rule.training.level = TrainingLevel::Frequent;
    return rule;
}

constexpr EvolutionRule happinessRule() {
    EvolutionRule rule{};
    rule.from = EvolutionStage::Child;
    rule.exits[0] = FormId::AdultB;
    rule.exitCount = 1;
    rule.mistakes.enabled = true;
    rule.mistakes.stage = EvolutionStage::Child;
    rule.mistakes.maximum = balance::kFixtureHappinessMaxMistakes;
    rule.happinessGood.enabled = true;
    rule.happinessGood.stage = EvolutionStage::Child;
    rule.happinessGood.minPercent = balance::kFixtureHappinessBranchPercent;
    return rule;
}

constexpr EvolutionRule fallbackRule() {
    EvolutionRule rule{};
    rule.from = EvolutionStage::Child;
    rule.exits[0] = FormId::AdultC;
    rule.exitCount = 1;
    return rule;
}

constexpr EvolutionRule kChildToAdultRules[] = {
    specialRule(),
    frequentRule(),
    happinessRule(),
    fallbackRule(),
};

}  // namespace

const StageHistory& EvolutionContext::stageHistory(EvolutionStage stage) const noexcept {
    if (care == nullptr) {
        static const StageHistory empty{};
        return empty;
    }
    return historyFor(*care, stage);
}

std::uint16_t EvolutionContext::lifetimeCareMistakes() const noexcept {
    return care == nullptr ? 0 : care->lifetimeCareMistakes;
}

bool ruleMatches(const EvolutionRule& rule, const EvolutionContext& context) noexcept {
    return mistakesMatch(rule.mistakes, context) && trainingMatches(rule.training, context) &&
           ratioMatches(rule.healthGood, context, true) &&
           ratioMatches(rule.happinessGood, context, false);
}

FormId resolveEvolution(const EvolutionContext& context, IRandom& random) {
    for (const auto& rule : kChildToAdultRules) {
        if (rule.from != context.leaving) {
            continue;
        }
        if (!ruleMatches(rule, context)) {
            continue;
        }
        if (rule.exitCount <= 1) {
            return rule.exits[0];
        }
        const auto index = random.nextBounded(rule.exitCount);
        return rule.exits[index < 2 ? index : 0];
    }
    return FormId::None;
}

}  // namespace neripal::core
