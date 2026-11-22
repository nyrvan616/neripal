#pragma once

#include <cstdint>
#include <optional>

namespace neripal::platform {

enum class InputAction {
    Next,
    Confirm,
    Back,
    // All three device buttons, or Z+X+C, held for kResetChordMs. One shot.
    ResetPrompt,
};

inline constexpr std::uint32_t kResetChordMs = 3000;

class IInput {
public:
    virtual ~IInput() = default;
    virtual std::optional<InputAction> pollAction() = 0;
};

}  // namespace neripal::platform
