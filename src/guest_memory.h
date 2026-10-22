#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

#include <rex/ppc.h>

namespace crackdown::guest {

// Guest pointers below this are never objects.
inline constexpr uint32_t kMinObjectAddress = 0x10000;
inline constexpr uint32_t kMaxObjectAddress = 0xE0000000;

inline bool Plausible(uint32_t address) {
  return address >= kMinObjectAddress && address < kMaxObjectAddress && (address & 3) == 0;
}

inline uint32_t Load32(uint8_t* base, uint32_t addr) {
  uint32_t value = 0;
  std::memcpy(&value, rex::memory::GuestPtr<const uint8_t*>(base, addr), sizeof(value));
  return std::byteswap(value);
}

inline void Store32(uint8_t* base, uint32_t addr, uint32_t value) {
  value = std::byteswap(value);
  std::memcpy(rex::memory::GuestPtr<uint8_t*>(base, addr), &value, sizeof(value));
}

inline float LoadFloat(uint8_t* base, uint32_t addr) { return std::bit_cast<float>(Load32(base, addr)); }

inline void StoreFloat(uint8_t* base, uint32_t addr, float value) { Store32(base, addr, std::bit_cast<uint32_t>(value)); }

inline uint8_t Load8(uint8_t* base, uint32_t addr) { return *rex::memory::GuestPtr<const uint8_t*>(base, addr); }

inline void Store8(uint8_t* base, uint32_t addr, uint8_t value) { *rex::memory::GuestPtr<uint8_t*>(base, addr) = value; }

}  // namespace crackdown::guest
