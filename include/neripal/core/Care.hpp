#pragma once

namespace neripal::core {

enum class CareAction {
    Feed,
    Train,
    Sleep,
    Wake,
    Clean,
};

enum class CareResult {
    Applied,
    RejectedAsleep,
    RejectedNoEnergy,
    RejectedAlreadySleeping,
    RejectedAlreadyAwake,
};

}  // namespace neripal::core
