#pragma once

#include "neripal/core/Evolution.hpp"

#include <cstdint>

namespace neripal::core {

// One life-stage crossing for Presentation. Not a GameEvent: that ring is
// cleared on restore and drops old entries. Confirming a notice does not
// change stage or form. Save V2 will serialize the queue; it already survives
// an in-memory snapshot.
inline constexpr std::uint8_t kEvolutionNoticeCapacity = 4;

struct EvolutionNotice {
    EvolutionStage from = EvolutionStage::Egg;
    EvolutionStage to = EvolutionStage::Egg;
    FormId form = FormId::None;
};

struct EvolutionNoticeQueue {
    void push(const EvolutionNotice& notice) noexcept {
        if (count_ == kEvolutionNoticeCapacity) {
            head_ = static_cast<std::uint8_t>((head_ + 1u) % kEvolutionNoticeCapacity);
            --count_;
        }
        const auto tail =
            static_cast<std::uint8_t>((head_ + count_) % kEvolutionNoticeCapacity);
        items_[tail] = notice;
        ++count_;
    }

    bool peek(EvolutionNotice& out) const noexcept {
        if (count_ == 0) {
            return false;
        }
        out = items_[head_];
        return true;
    }

    bool confirm() noexcept {
        if (count_ == 0) {
            return false;
        }
        head_ = static_cast<std::uint8_t>((head_ + 1u) % kEvolutionNoticeCapacity);
        --count_;
        return true;
    }

    void clear() noexcept {
        head_ = 0;
        count_ = 0;
    }

    std::uint8_t count() const noexcept { return count_; }

    EvolutionNotice at(std::uint8_t index) const noexcept {
        const auto slot =
            static_cast<std::uint8_t>((head_ + index) % kEvolutionNoticeCapacity);
        return items_[slot];
    }

private:
    EvolutionNotice items_[kEvolutionNoticeCapacity]{};
    std::uint8_t head_ = 0;
    std::uint8_t count_ = 0;
};

}  // namespace neripal::core
