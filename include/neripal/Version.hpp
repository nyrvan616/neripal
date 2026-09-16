#pragma once

#include <string_view>

namespace neripal::version {

#define NERIPAL_VERSION_TEXT "0.2.0"

inline constexpr int kMajor = 0;
inline constexpr int kMinor = 2;
inline constexpr int kPatch = 0;
inline constexpr std::string_view kString = NERIPAL_VERSION_TEXT;
inline constexpr std::string_view kDisplayName = "NeriPal " NERIPAL_VERSION_TEXT;
inline constexpr std::string_view kScreenLabel = "NERIPAL " NERIPAL_VERSION_TEXT;

#undef NERIPAL_VERSION_TEXT

}  // namespace neripal::version
