#include "settings.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <utility>
#include <vector>

#include <toml++/toml.hpp>

#include <rex/logging.h>

#include "trg/settings.h"

#include "aspect_ratio.h"
#include "cheats.h"
#include "frame_timing.h"
#include "graphics_adapters.h"
#include "internal_render_scale.h"
#include "keybinds.h"
#include "language_setting.h"
#include "mouse_look.h"
#include "multisampling.h"
#include "platform.h"
#include "presentation_setting.h"
#include "settings_file.h"

REXCVAR_DEFINE_BOOL(crackdown_launcher, true, "Crackdown",
                    "Show the launcher before the game starts (hold Shift at start to force it)");
REXCVAR_DEFINE_BOOL(crackdown_skip_launcher, false, "Crackdown",
                    "Skip the launcher once (set when the launcher restarts the game)");
REXCVAR_DEFINE_INT32(crackdown_frame_rate, 60, "Crackdown",
                     "Frame rate: 30 (the console's), 60, 90, 120, 144, 165, 240, or 0 for unlimited; other values "
                     "use 60");
REXCVAR_DEFINE_BOOL(crackdown_show_fps, true, "Crackdown", "Show the frame rate in the window title");
REXCVAR_DEFINE_BOOL(crackdown_show_render_resolution, false, "Crackdown",
                    "Show the render resolution in the window title");
REXCVAR_DEFINE_BOOL(crackdown_show_frame_counter, false, "Crackdown",
                    "Show the frame rate in the corner (F2 toggles it while playing)");
REXCVAR_DEFINE_BOOL(crackdown_msaa_compatibility, false, "Crackdown",
                    "Draw the game's 2x MSAA render targets with 4x MSAA, for GPUs or drivers whose native 2x "
                    "MSAA misrenders (sets native_2x_msaa)");
REXCVAR_DEFINE_STRING(crackdown_graphics_adapter, "auto", "Crackdown",
                      "Graphics card: auto, or the index d3d12_adapter takes (applied at start)");
REXCVAR_DEFINE_STRING(crackdown_aspect_ratio, "16:9", "Crackdown",
                      "Aspect ratio the game renders in: 16:9, 16:10, 21:9, 32:9, 4:3 or 5:4 (applied at start)");
REXCVAR_DEFINE_STRING(crackdown_language, "en-US", "Crackdown",
                      "Game language as a Local folder name, such as en-US or fr-FR (applied at start)");
REXCVAR_DEFINE_INT32(crackdown_launcher_x, 0, "Crackdown", "Launcher window left edge, written by the launcher");
REXCVAR_DEFINE_INT32(crackdown_launcher_y, 0, "Crackdown", "Launcher window top edge, written by the launcher");
REXCVAR_DEFINE_INT32(crackdown_launcher_width, 0, "Crackdown",
                     "Launcher window restored width, written by the launcher");
REXCVAR_DEFINE_INT32(crackdown_launcher_height, 0, "Crackdown",
                     "Launcher window restored height, written by the launcher");
REXCVAR_DEFINE_BOOL(crackdown_launcher_maximized, false, "Crackdown", "Launcher window was maximized when closed");
REXCVAR_DEFINE_BOOL(cheat_infinite_health, false, "Crackdown", "Infinite Health cheat");
REXCVAR_DEFINE_INT32(cheat_movement_speed, 0, "Crackdown", "Movement Speed cheat: 0 (Off), 2, 4, 6, 8 or 10");
REXCVAR_DEFINE_INT32(cheat_jump_height, 0, "Crackdown", "Jump Height cheat: 0 (Off), 2, 4, 6, 8 or 10");
REXCVAR_DEFINE_BOOL(cheat_no_fall_damage, false, "Crackdown", "No Fall Damage cheat");
REXCVAR_DEFINE_BOOL(cheat_infinite_ammo, false, "Crackdown", "Infinite Ammo cheat");
REXCVAR_DEFINE_BOOL(cheat_infinite_vehicle_health, false, "Crackdown", "Infinite Vehicle Health cheat");
REXCVAR_DEFINE_INT32(cheat_vehicle_speed, 0, "Crackdown", "Vehicle Speed cheat: 0 (Off), 2, 4 or 6");
REXCVAR_DEFINE_INT32(cheat_agility_orbs_on_map, 0, "Crackdown",
                     "Uncollected agility orbs: 0 (Off), 1 (Minimap) or 2 (Minimap and world map)");
REXCVAR_DEFINE_INT32(cheat_hidden_orbs_on_map, 0, "Crackdown",
                     "Uncollected hidden orbs: 0 (Off), 1 (Minimap) or 2 (Minimap and world map)");
REXCVAR_DEFINE_BOOL(crackdown_launcher_fullscreen, false, "Crackdown",
                    "Launcher was left in its own F11 fullscreen, written by the launcher");

namespace crackdown {
namespace {

const std::vector<std::string> kGameSettingKeys = [] {
  std::vector<std::string> keys = {
      "crackdown_launcher",
      "crackdown_skip_intro",
      "crackdown_frame_rate",
      "crackdown_show_fps",
      "crackdown_show_render_resolution",
      "crackdown_show_frame_counter",
      "fullscreen",
      "window_width",
      "window_height",
      "monitor",
      kTearingSetting,
      "crackdown_graphics_adapter",
      "crackdown_aspect_ratio",
      kLanguageSetting,
      kLetterboxSetting,
      "resolution_scale",
      "internal_render_scale",
      "swap_post_effect",
      "crackdown_msaa_compatibility",
      "crackdown_detailed_logging",
      "anisotropic_override",
      "mnk_mode",
      "mnk_sensitivity",
      "cheat_infinite_health",
      "cheat_movement_speed",
      "cheat_jump_height",
      "cheat_no_fall_damage",
      "cheat_infinite_ammo",
      "cheat_infinite_vehicle_health",
      "cheat_vehicle_speed",
      "cheat_agility_orbs_on_map",
      "cheat_hidden_orbs_on_map",
  };
  for (const KeyboardAction& action : kKeyboardActions) {
    keys.emplace_back(action.key);
  }
  return keys;
}();

const std::vector<std::string> kWindowKeys = {
    "window_width",
    "window_height",
    "monitor",
    kTearingSetting,
    "crackdown_graphics_adapter",
};

// Cheats are read while the game runs, so changing one never needs a new process.
bool IsCheatKey(std::string_view key) { return key.substr(0, 6) == "cheat_"; }

// Settings the running session reads itself, so changing one never needs a new process.
bool IsLiveKey(std::string_view key) { return IsCheatKey(key) || key == "crackdown_detailed_logging"; }

bool IsWindowKey(std::string_view key) {
  return std::find(kWindowKeys.begin(), kWindowKeys.end(), key) != kWindowKeys.end();
}

// Monitor 0 (Default) is the primary display, as monitor 1 is.
std::string WindowValue(std::string_view key, const std::string& value) {
  if (key == "monitor") {
    return std::to_string(std::max(1, std::atoi(value.c_str())));
  }
  return value;
}

std::string ClampedSensitivity(const std::string& value) {
  const double parsed = std::atof(value.c_str());
  return std::to_string(
      std::clamp(parsed > 0.0 ? parsed : 1.0, mouse_look::kMinSensitivity, mouse_look::kMaxSensitivity));
}

std::string TomlValue(std::string_view key, const std::string& value) {
  const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(key);
  if (!info || info->type != rex::cvar::FlagType::String) {
    return value;
  }
  std::string quoted = "\"";
  for (char c : value) {
    if (c == '"' || c == '\\') {
      quoted += '\\';
    }
    quoted += c;
  }
  return quoted + '"';
}

std::optional<std::string> ConfigValue(const toml::table& table, std::string_view key) {
  const toml::node* node = table.get(key);
  if (!node) {
    return std::nullopt;
  }
  if (auto v = node->value_exact<bool>()) {
    return *v ? "true" : "false";
  }
  if (auto v = node->value_exact<int64_t>()) {
    return std::to_string(*v);
  }
  if (auto v = node->value_exact<double>()) {
    return std::to_string(*v);
  }
  return node->value_exact<std::string>();
}

trg::TextFileSettings OpenPlayerFile(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (!std::filesystem::exists(path, ec)) {
    std::ofstream(path) << "# Crackdown player settings, written by the launcher.\n"
                           "# Loaded after crackdown.toml. Remove a line to use the shipped value.\n";
  }
  return trg::TextFileSettings(Utf8(path));
}

void LogSettingsCheck(const std::filesystem::path& path, const SettingsFileResult& result) {
  switch (result.state) {
    case SettingsFileState::kRepaired:
      REXLOG_WARN("Removed {} invalid line(s) from {}; the original is kept as {}", result.dropped.size(), Utf8(path),
                  Utf8(result.backup));
      for (const std::string& line : result.dropped) {
        REXLOG_WARN("  {}", line);
      }
      break;
    case SettingsFileState::kQuarantined:
      REXLOG_ERROR("{} could not be repaired and was moved to {}; the default settings apply", Utf8(path),
                   Utf8(result.backup));
      break;
    case SettingsFileState::kUnreadable:
      REXLOG_ERROR("Cannot use {}: {}", Utf8(path), result.detail);
      break;
    case SettingsFileState::kMissing:
    case SettingsFileState::kValid:
      break;
  }
}

}  // namespace

void LoadPlayerSettings(const std::filesystem::path& path) {
  const SettingsFileResult result = CheckPlayerSettings(path);
  LogSettingsCheck(path, result);
  if (result.state == SettingsFileState::kRepaired || result.state == SettingsFileState::kValid) {
    rex::cvar::LoadConfig(path);
    REXLOG_INFO("Player settings: {}", Utf8(path));
  }
  SyncSavedCheats();
}

void SyncSavedCheats() {
  using cheats::Cheat;
  cheats::SetSavedValue(Cheat::InfiniteHealth, REXCVAR_GET(cheat_infinite_health) ? 1 : 0);
  cheats::SetSavedValue(Cheat::MovementSpeed, REXCVAR_GET(cheat_movement_speed));
  cheats::SetSavedValue(Cheat::JumpHeight, REXCVAR_GET(cheat_jump_height));
  cheats::SetSavedValue(Cheat::NoFallDamage, REXCVAR_GET(cheat_no_fall_damage) ? 1 : 0);
  cheats::SetSavedValue(Cheat::InfiniteAmmo, REXCVAR_GET(cheat_infinite_ammo) ? 1 : 0);
  cheats::SetSavedValue(Cheat::InfiniteVehicleHealth, REXCVAR_GET(cheat_infinite_vehicle_health) ? 1 : 0);
  cheats::SetSavedValue(Cheat::VehicleSpeed, REXCVAR_GET(cheat_vehicle_speed));
  cheats::SetSavedValue(Cheat::AgilityOrbsOnMap, REXCVAR_GET(cheat_agility_orbs_on_map));
  cheats::SetSavedValue(Cheat::HiddenOrbsOnMap, REXCVAR_GET(cheat_hidden_orbs_on_map));
}

WindowPlacement SavedLauncherPlacement() {
  struct Cvars {
    int GetInt(const char* key, int) const { return std::atoi(rex::cvar::GetFlagByName(key).c_str()); }
    bool GetBool(const char* key, bool) const { return rex::cvar::GetFlagByName(key) == "true"; }
  };
  return ReadPlacement(Cvars{});
}

bool SavedLauncherFullscreen() {
  return REXCVAR_GET(crackdown_launcher_fullscreen);
}

namespace {

constexpr double kGameWindowDisplayShare = 0.9;

std::pair<int, int> FitWindowSize(int width, int height, const MonitorInfo& display) {
  if (display.work_width <= 0 || display.work_height <= 0 || display.scale <= 0.0f) {
    return {width, height};
  }
  const int max_width = int(display.work_width * kGameWindowDisplayShare / display.scale);
  const int max_height = int(display.work_height * kGameWindowDisplayShare / display.scale);
  return {std::min(width, max_width), std::min(height, max_height)};
}

}  // namespace

void FitWindowToDisplay() {
  const int width = std::atoi(rex::cvar::GetFlagByName("window_width").c_str());
  const int height = std::atoi(rex::cvar::GetFlagByName("window_height").c_str());
  if (width <= 0 || height <= 0) {
    return;
  }
  const MonitorInfo display = MonitorForSetting(std::atoi(rex::cvar::GetFlagByName("monitor").c_str()));
  if (display.width == 0) {
    return;
  }
  const auto [fit_width, fit_height] = FitWindowSize(width, height, display);
  if (fit_width == width && fit_height == height) {
    return;
  }
  REXLOG_INFO("Window reduced from {}x{} to {}x{} to fit the display", width, height, fit_width, fit_height);
  rex::cvar::SetFlagByName("window_width", std::to_string(fit_width));
  rex::cvar::SetFlagByName("window_height", std::to_string(fit_height));
}

void GameSettings::Capture(const std::filesystem::path& player_file, const std::filesystem::path& shipped_config) {
  player_file_ = player_file;
  shipped_config_ = shipped_config;
  saved_.clear();
  unregistered_.clear();
  for (const std::string& key : kGameSettingKeys) {
    saved_[key] = rex::cvar::GetFlagByName(key);
    if (key == "crackdown_frame_rate") {
      saved_[key] = std::to_string(ParseFrameRate(saved_[key]));
    }
    if (!rex::cvar::GetFlagInfo(key)) {
      unregistered_.insert(key);
    }
  }
  pending_ = saved_;
}

void GameSettings::CaptureLateRegistered() {
  for (auto it = unregistered_.begin(); it != unregistered_.end();) {
    const std::string& key = *it;
    if (!rex::cvar::GetFlagInfo(key)) {
      ++it;
      continue;
    }
    if (pending_[key].empty()) {
      saved_[key] = pending_[key] = rex::cvar::GetFlagByName(key);
    }
    it = unregistered_.erase(it);
  }
}

std::string GameSettings::Get(std::string_view key, std::string_view def) const {
  const auto it = pending_.find(std::string(key));
  return it == pending_.end() || it->second.empty() ? std::string(def) : it->second;
}

void GameSettings::Set(std::string_view key, std::string value) {
  pending_[std::string(key)] = std::move(value);
}

bool GameSettings::Save() {
  std::vector<std::pair<std::string, std::string>> changes;
  for (const auto& [key, value] : pending_) {
    const auto saved = saved_.find(key);
    if (saved == saved_.end() || saved->second != value) {
      changes.emplace_back(key, value);
    }
  }
  if (changes.empty()) {
    return true;
  }
  for (const auto& [key, value] : changes) {
    if (!rex::cvar::GetFlagInfo(key)) {
      REXLOG_ERROR("Cannot save {}: the setting is not registered", key);
      return false;
    }
  }

  trg::TextFileSettings file = OpenPlayerFile(player_file_);
  for (const auto& [key, value] : changes) {
    file.Set(key, TomlValue(key, value));
  }
  if (!file.Save()) {
    REXLOG_ERROR("Cannot write player settings to {}", Utf8(player_file_));
    return false;
  }
  saved_ = pending_;
  return true;
}

bool GameSettings::SaveNow(const SettingValues& values) {
  trg::TextFileSettings file = OpenPlayerFile(player_file_);
  for (const auto& [key, value] : values) {
    file.Set(key, value);
  }
  if (!file.Save()) {
    REXLOG_ERROR("Cannot write player settings to {}", Utf8(player_file_));
    return false;
  }
  return true;
}

bool GameSettings::SaveNow(std::string_view key, std::string value) {
  const std::string name(key);
  if (!SaveNow(SettingValues{{name, value}})) {
    return false;
  }
  saved_[name] = pending_[name] = std::move(value);
  return true;
}

void GameSettings::ResetToDefaults() {
  toml::table shipped;
  try {
    shipped = toml::parse_file(Utf8(shipped_config_));
  } catch (const toml::parse_error& e) {
    REXLOG_WARN("Cannot read {}: {}", Utf8(shipped_config_), e.what());
  }
  for (const std::string& key : kGameSettingKeys) {
    const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(key);
    pending_[key] = ConfigValue(shipped, key).value_or(info ? info->default_value : std::string());
  }
}

void GameSettings::Apply() const {
  for (const auto& [key, value] : pending_) {
    if (IsWindowKey(key) || key == kFullscreenKey || rex::cvar::GetFlagByName(key) == value) {
      continue;
    }
    rex::cvar::SetFlagByName(key, key == "mnk_sensitivity" ? ClampedSensitivity(value) : value);
  }
  ApplyAspectRatio();
  ApplyInternalRenderPath();
  ApplyMultisampling();
  mouse_look::Configure();
  SyncSavedCheats();
}

void GameSettings::ApplyDisplayMode() const {
  const auto pending = pending_.find(kFullscreenKey);
  if (pending == pending_.end() || rex::cvar::GetFlagByName(kFullscreenKey) == pending->second) {
    return;
  }
  rex::cvar::SetFlagByName(kFullscreenKey, pending->second);
}

void ApplyMultisampling() {
  rex::cvar::SetFlagByName("native_2x_msaa",
                           Native2xMsaaFor(REXCVAR_GET(crackdown_msaa_compatibility)) ? "true" : "false");
}

// ReXGlue takes the guest video mode from the window cvars while a video_mode cvar holds its default. The window has
// consumed them already, so clearing them leaves the video_mode cvars in charge.
void ApplyAspectRatio() {
  const AspectRatio aspect = ParseAspectRatio(REXCVAR_GET(crackdown_aspect_ratio));
  const std::optional<WindowSize> picture = PinnedVideoMode(aspect);
  if (!picture) {
    return;
  }
  rex::cvar::SetFlagByName("video_mode_width", std::to_string(picture->width));
  rex::cvar::SetFlagByName("video_mode_height", std::to_string(picture->height));
  rex::cvar::SetFlagByName("window_width", "0");
  rex::cvar::SetFlagByName("window_height", "0");
  REXLOG_INFO("Aspect ratio: {} ({}x{})", InfoFor(aspect).key, picture->width, picture->height);
}

void ApplyInternalRenderPath() {
#if defined(_WIN32)
  const std::optional<std::string_view> path =
      RequiredRenderTargetPath(std::atoi(rex::cvar::GetFlagByName("internal_render_scale").c_str()),
                               rex::cvar::GetFlagByName("render_target_path_d3d12"));
  if (path && rex::cvar::SetFlagByName("render_target_path_d3d12", *path)) {
    REXLOG_INFO("Internal render resolution: using D3D12 host render targets");
  }
#endif
}

void ApplyGraphicsAdapter() {
#if defined(_WIN32)
  const std::string setting = REXCVAR_GET(crackdown_graphics_adapter);
  if (!ParseGraphicsAdapter(setting)) {
    REXLOG_INFO("Graphics card: Automatic");
    return;
  }
  const std::vector<GraphicsAdapter> adapters = ListGraphicsAdapters();
  const int index = ResolveGraphicsAdapter(setting, adapters);
  if (index < 0) {
    REXLOG_WARN("Graphics card {} is not available, using Automatic", setting);
    return;
  }
  rex::cvar::SetFlagByName("d3d12_adapter", std::to_string(index));
  for (const GraphicsAdapter& adapter : adapters) {
    if (adapter.index == index) {
      REXLOG_INFO("Graphics card: {} (adapter {})", adapter.name, index);
    }
  }
#endif
}

void GameSettings::MarkWindowCreated() {
  window_created_.clear();
  for (const std::string& key : kWindowKeys) {
    window_created_[key] = rex::cvar::GetFlagByName(key);
  }
}

bool GameSettings::WindowChanged() const {
  for (const std::string& key : kWindowKeys) {
    const auto created = window_created_.find(key);
    const std::string created_value = created == window_created_.end() ? std::string() : created->second;
    if (WindowValue(key, Get(key)) != WindowValue(key, created_value)) {
      return true;
    }
  }
  return false;
}

void GameSettings::MarkRuntimeBuilt() { runtime_built_ = pending_; }

bool GameSettings::RuntimeChanged() const {
  const auto value_of = [](const std::map<std::string, std::string>& values, const std::string& key) {
    const auto found = values.find(key);
    return found == values.end() ? std::string() : found->second;
  };
  for (const std::string& key : kGameSettingKeys) {
    if (key != kFullscreenKey && !IsLiveKey(key) && value_of(pending_, key) != value_of(runtime_built_, key)) {
      return true;
    }
  }
  return false;
}

}  // namespace crackdown
