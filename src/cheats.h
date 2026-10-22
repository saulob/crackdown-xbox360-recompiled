#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace crackdown::cheats {

enum class Cheat : uint8_t {
  InfiniteHealth,
  MovementSpeed,
  JumpHeight,
  NoFallDamage,
  InfiniteAmmo,
  InfiniteVehicleHealth,
  VehicleSpeed,
  AgilityOrbsOnMap,
  HiddenOrbsOnMap,
};

enum class Section : uint8_t { Player, Weapons, Vehicle, Map };

// A toggle is Off or On. A multiplier is Off or one of its listed factors. A map display is Off, the minimap, or the
// minimap and the world map.
enum class Kind : uint8_t { Toggle, Multiplier, MapDisplay };

inline constexpr int kMapOff = 0;
inline constexpr int kMapMinimap = 1;
inline constexpr int kMapMinimapAndWorld = 2;

inline constexpr std::array kToggleValues = {0, 1};
inline constexpr std::array kMovementValues = {0, 2, 4, 6, 8, 10};
inline constexpr std::array kVehicleValues = {0, 2, 4, 6};
inline constexpr std::array kMapValues = {kMapOff, kMapMinimap, kMapMinimapAndWorld};

struct CheatInfo {
  Cheat cheat;
  Section section;
  Kind kind;
  const char* label;
  const char* key;
  std::span<const int> values;
};

inline constexpr std::array kCheats = {
    CheatInfo{Cheat::InfiniteHealth, Section::Player, Kind::Toggle, "Infinite Health", "cheat_infinite_health",
              kToggleValues},
    CheatInfo{Cheat::MovementSpeed, Section::Player, Kind::Multiplier, "Movement Speed", "cheat_movement_speed",
              kMovementValues},
    CheatInfo{Cheat::JumpHeight, Section::Player, Kind::Multiplier, "Jump Height", "cheat_jump_height",
              kMovementValues},
    CheatInfo{Cheat::NoFallDamage, Section::Player, Kind::Toggle, "No Fall Damage", "cheat_no_fall_damage",
              kToggleValues},
    CheatInfo{Cheat::InfiniteAmmo, Section::Weapons, Kind::Toggle, "Infinite Ammo", "cheat_infinite_ammo",
              kToggleValues},
    CheatInfo{Cheat::InfiniteVehicleHealth, Section::Vehicle, Kind::Toggle, "Infinite Vehicle Health",
              "cheat_infinite_vehicle_health", kToggleValues},
    CheatInfo{Cheat::VehicleSpeed, Section::Vehicle, Kind::Multiplier, "Vehicle Speed", "cheat_vehicle_speed",
              kVehicleValues},
    CheatInfo{Cheat::AgilityOrbsOnMap, Section::Map, Kind::MapDisplay, "Agility Orbs on Map",
              "cheat_agility_orbs_on_map", kMapValues},
    CheatInfo{Cheat::HiddenOrbsOnMap, Section::Map, Kind::MapDisplay, "Hidden Orbs on Map", "cheat_hidden_orbs_on_map",
              kMapValues},
};

inline constexpr size_t kCheatCount = kCheats.size();

inline constexpr const char* SectionLabel(Section section) {
  switch (section) {
    case Section::Player:
      return "PLAYER";
    case Section::Weapons:
      return "WEAPONS";
    case Section::Vehicle:
      return "VEHICLE";
    case Section::Map:
      return "MAP";
  }
  return "";
}

inline constexpr const CheatInfo& InfoOf(Cheat cheat) { return kCheats[static_cast<size_t>(cheat)]; }

// The value after `value` in the cheat's list, wrapping to Off after the last one.
constexpr int NextValue(const CheatInfo& info, int value) {
  for (size_t i = 0; i < info.values.size(); ++i) {
    if (info.values[i] == value) {
      return info.values[(i + 1) % info.values.size()];
    }
  }
  return info.values.front();
}

// Off, On for a toggle, the factor as "2x" for a multiplier, or Minimap / Minimap + World for a map display.
const char* ValueLabel(Kind kind, int value);

// The value in effect: this session's change, or else the launcher's saved value.
int Value(Cheat cheat);
bool Enabled(Cheat cheat);
int Factor(Cheat cheat);

// Changes the cheat for this session only and returns the new value. Turning a cheat on asks for a refill.
int Cycle(Cheat cheat);

// The launcher's saved value. A value the cheat does not offer counts as Off.
void SetSavedValue(Cheat cheat, int value);
// Drops every change made this session, so the saved values apply again.
void ResetSession();

// A cheat turned on asks the game thread to refill once; the request is consumed by the first thread to take it.
bool TakeRefillRequest(Cheat cheat);
void RequestRefill(Cheat cheat);

}  // namespace crackdown::cheats
