#pragma once

#include "neripal/core/Activity.hpp"

#include <cstdint>

namespace neripal::core {

enum class GameEventKind : std::uint8_t {
    ActivityStarted,
    ActivityFinished,
};

// Consume-once activity edge. There is no Step kind: presentation reads pose
// from PetState, not from a stream of ticks.
struct GameEvent {
    GameEventKind kind = GameEventKind::ActivityStarted;
    Activity activity = Activity::Idle;
    IdleVariant idleVariant = IdleVariant::Bob;
    bool completed = false;  // ActivityFinished: true if the episode ended on its own
};

inline constexpr std::uint8_t kGameEventCapacity = 4;
static_assert(kGameEventCapacity >= 2 && kGameEventCapacity <= 4,
              "0.4 event ring is a fixed 2–4 slot buffer");

// Embedded FIFO. Overflow drops the oldest event; the buffer never grows.
struct GameEventQueue {
    void push(const GameEvent& event) noexcept {
        if (count_ == kGameEventCapacity) {
            head_ = static_cast<std::uint8_t>((head_ + 1u) % kGameEventCapacity);
            --count_;
        }
        const std::uint8_t tail =
            static_cast<std::uint8_t>((head_ + count_) % kGameEventCapacity);
        items_[tail] = event;
        ++count_;
    }

    bool poll(GameEvent& out) noexcept {
        if (count_ == 0) {
            return false;
        }
        out = items_[head_];
        head_ = static_cast<std::uint8_t>((head_ + 1u) % kGameEventCapacity);
        --count_;
        return true;
    }

    void clear() noexcept {
        head_ = 0;
        count_ = 0;
    }

    std::uint8_t size() const noexcept { return count_; }

private:
    GameEvent items_[kGameEventCapacity]{};
    std::uint8_t head_ = 0;
    std::uint8_t count_ = 0;
};

}  // namespace neripal::core
