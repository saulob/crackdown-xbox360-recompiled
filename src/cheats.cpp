#include "cheats.h"

#include <algorithm>
#include <atomic>

namespace crackdown::cheats {
namespace {

std::array<std::atomic<int>, kCheatCount> g_saved{};
std::array<std::atomic<int>, kCheatCount> g_session{};
std::array<std::atomic<bool>, kCheatCount> g_overridden{};
std::array<std::atomic<bool>, kCheatCount> g_refill{};

size_t IndexOf(Cheat cheat) { return static_cast<size_t>(cheat); }

bool Offers(const CheatInfo& info, int value) {
  return std::find(info.values.begin(), info.values.end(), value) != info.values.end();
}

}  // namespace

const char* ValueLabel(Kind kind, int value) {
  if (value == 0) {
    return "Off";
  }
  if (kind == Kind::Toggle) {
    return "On";
  }
  if (kind == Kind::MapDisplay) {
    return value == kMapMinimap ? "Minimap" : "Minimap + World";
  }
  switch (value) {
    case 2:
      return "2x";
    case 4:
      return "4x";
    case 6:
      return "6x";
    case 8:
      return "8x";
    case 10:
      return "10x";
  }
  return "Off";
}

int Value(Cheat cheat) {
  const size_t index = IndexOf(cheat);
  if (g_overridden[index].load(std::memory_order_acquire)) {
    return g_session[index].load(std::memory_order_relaxed);
  }
  return g_saved[index].load(std::memory_order_relaxed);
}

bool Enabled(Cheat cheat) { return Value(cheat) != 0; }

int Factor(Cheat cheat) {
  const int value = Value(cheat);
  return value > 0 ? value : 1;
}

int Cycle(Cheat cheat) {
  const size_t index = IndexOf(cheat);
  const int next = NextValue(InfoOf(cheat), Value(cheat));
  g_session[index].store(next, std::memory_order_relaxed);
  g_overridden[index].store(true, std::memory_order_release);
  if (next != 0) {
    RequestRefill(cheat);
  }
  return next;
}

void SetSavedValue(Cheat cheat, int value) {
  const CheatInfo& info = InfoOf(cheat);
  g_saved[IndexOf(cheat)].store(Offers(info, value) ? value : 0, std::memory_order_relaxed);
}

void ResetSession() {
  for (size_t i = 0; i < kCheatCount; ++i) {
    g_overridden[i].store(false, std::memory_order_release);
    g_refill[i].store(false, std::memory_order_relaxed);
  }
}

bool TakeRefillRequest(Cheat cheat) { return g_refill[IndexOf(cheat)].exchange(false, std::memory_order_acq_rel); }

void RequestRefill(Cheat cheat) { g_refill[IndexOf(cheat)].store(true, std::memory_order_release); }

}  // namespace crackdown::cheats
