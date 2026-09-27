#pragma once

namespace neripal::core {

enum class CareAction {
    Feed,
    Train,
    Sleep,
    Wake,
    Clean,
    Pet,
    Play,
};

enum class CareResult {
    Applied,
    RejectedAsleep,
    RejectedNoEnergy,
    RejectedAlreadySleeping,
    RejectedAlreadyAwake,
    RejectedEgg,
};

}  // namespace neripal::core
