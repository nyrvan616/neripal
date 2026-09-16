#pragma once

#include <optional>

namespace neripal::platform {

enum class InputAction {
    Feed,
    Train,
    Sleep,
    Wake,
    Reset,
};

class IInput {
public:
    virtual ~IInput() = default;
    virtual std::optional<InputAction> pollAction() = 0;
};

}  // namespace neripal::platform
