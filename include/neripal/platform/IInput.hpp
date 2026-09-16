#pragma once

#include <optional>

namespace neripal::platform {

enum class InputAction {
    Next,
    Confirm,
    Back,
};

class IInput {
public:
    virtual ~IInput() = default;
    virtual std::optional<InputAction> pollAction() = 0;
};

}  // namespace neripal::platform
