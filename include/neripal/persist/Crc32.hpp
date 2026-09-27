#pragma once

#include <cstddef>
#include <cstdint>

namespace neripal::persist {

// CRC-32 ISO-HDLC (poly 0xEDB88320). Vector "123456789" -> 0xCBF43926.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace neripal::persist
