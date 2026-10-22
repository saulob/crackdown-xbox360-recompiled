#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include <cstdint>

#include <rex/cvar.h>

#include "trg/settings.h"

#include "window_placement.h"

REXCVAR_DECLARE(bool, crackdown_launcher);
REXCVAR_DECLARE(bool, crackdown_skip_launcher);
REXCVAR_DECLARE(bool, crackdown_skip_intro);
REXCVAR_DECLARE(int32_t, crackdown_frame_rate);
REXCVAR_DECLARE(bool, crackdown_show_fps);
REXCVAR_DECLARE(bool, crackdown_show_render_resolution);
REXCVAR_DECLARE(bool, crackdown_show_frame_counter);
REXCVAR_DECLARE(bool, crackdown_msaa_compatibility);
REXCVAR_DECLARE(bool, crackdown_detailed_logging);
REXCVAR_DECLARE(std::string, crackdown_graphics_adapter);
REXCVAR_DECLARE(std::string, crackdown_aspect_ratio);
REXCVAR_DECLARE(std::string, crackdown_language);
REXCVAR_DECLARE(int32_t, crackdown_launcher_x);
REXCVAR_DECLARE(int32_t, crackdown_launcher_y);
REXCVAR_DECLARE(int32_t, crackdown_launcher_width);
REXCVAR_DECLARE(int32_t, crackdown_launcher_height);
REXCVAR_DECLARE(bool, crackdown_launcher_maximized);
REXCVAR_DECLARE(bool, crackdown_launcher_fullscreen);
REXCVAR_DECLARE(bool, cheat_infinite_health);
REXCVAR_DECLARE(int32_t, cheat_movement_speed);
REXCVAR_DECLARE(int32_t, cheat_jump_height);
REXCVAR_DECLARE(bool, cheat_no_fall_damage);
REXCVAR_DECLARE(bool, cheat_infinite_ammo);
REXCVAR_DECLARE(bool, cheat_infinite_vehicle_health);
REXCVAR_DECLARE(int32_t, cheat_vehicle_speed);
REXCVAR_DECLARE(int32_t, cheat_agility_orbs_on_map);
REXCVAR_DECLARE(int32_t, cheat_hidden_orbs_on_map);

namespace crackdown {

// Allowing tearing is VSync Off. Each platform's presenter has its own setting for it.
#if defined(_WIN32)
inline constexpr char kTearingSetting[] = "d3d12_allow_variable_refresh_rate_and_tearing";
#else
inline constexpr char kTearingSetting[] = "vulkan_allow_present_mode_immediate";
#endif
inline constexpr char kFullscreenKey[] = "fullscreen";

// The player's settings file, loaded over the shipped crackdown.toml.
inline std::filesystem::path PlayerSettingsPath(const std::filesystem::path& user_data_root) {
  return user_data_root / "settings.toml";
}

// The cheats the launcher saved become the values the game starts with.
void SyncSavedCheats();

// Makes the file loadable (ReXGlue discards a whole file for one invalid line), then loads it.
void LoadPlayerSettings(const std::filesystem::path& path);

// Sets the Xenos native_2x_msaa cvar from the launcher's Multisampling choice. Call once the GPU plugin has
// registered its cvars and before emulation starts.
void ApplyMultisampling();

// Sets the guest video mode to the Aspect Ratio's canvas, whatever the window size. 16:9 Original sets none. Call once
// the window exists and before emulation starts.
void ApplyAspectRatio();

// Windows: makes the D3D12 render target path the host one when Internal render resolution is below 100%. Call once
// the launcher choices are applied and before the command processor is created.
void ApplyInternalRenderPath();

// Sets d3d12_adapter from the launcher's Graphics card choice. Call once the player settings are loaded and before the
// graphics device is created; Automatic leaves the SDK's own preference alone.
void ApplyGraphicsAdapter();

// Caps the requested window size to the display the `monitor` setting names. Call before the window exists.
void FitWindowToDisplay();

// The launcher window placement loaded from the player file.
WindowPlacement SavedLauncherPlacement();
bool SavedLauncherFullscreen();

// Edits stay pending until Save() or Apply(), so nothing reaches the window while the launcher is open.
class GameSettings final : public trg::SettingsStore {
 public:
  // The settings file is TOML, so toggles are written as true and false.
  GameSettings() {
    true_value = "true";
    false_value = "false";
  }

  // Takes the settings in effect now as the saved state.
  void Capture(const std::filesystem::path& player_file, const std::filesystem::path& shipped_config);
  // Takes the settings that were not registered at Capture (the Xenos graphics cvars). Call once after
  // SetupPresentation and before the launcher is shown.
  void CaptureLateRegistered();

  std::string Get(std::string_view key, std::string_view def = {}) const override;
  void Set(std::string_view key, std::string value) override;
  bool Save() override;
  // Writes the given values at once. Pending edits stay pending, and Reset never touches them.
  bool SaveNow(const SettingValues& values);
  // Writes one setting now and keeps its saved and pending values in step.
  bool SaveNow(std::string_view key, std::string value);
  void ResetToDefaults() override;

  // Sets the running game's cvars that differ from the pending values, except the window mode.
  void Apply() const;
  // Sets the window mode from the pending value. Call once the launcher is no longer fullscreen.
  void ApplyDisplayMode() const;

  // Records the window settings this process creates its window with.
  void MarkWindowCreated();
  // Whether the pending window size, monitor, VSync or graphics card needs a new process.
  bool WindowChanged() const;
  // Records the pending settings the runtime is built with.
  void MarkRuntimeBuilt();
  // Whether a pending game setting differs from the one the runtime was built with.
  bool RuntimeChanged() const;

 private:
  std::filesystem::path player_file_;
  std::filesystem::path shipped_config_;
  std::map<std::string, std::string> saved_;
  std::map<std::string, std::string> pending_;
  std::map<std::string, std::string> window_created_;
  std::map<std::string, std::string> runtime_built_;
  std::set<std::string> unregistered_;
};

}  // namespace crackdown
