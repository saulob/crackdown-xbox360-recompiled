#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include <rex/filesystem.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/version.h>
#include <rex/system/kernel_state.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/windowed_app_context.h>

#include <trg/game_helpers.h>

#include "achievements_input.h"
#include "achievements_overlay.h"
#include "active_log.h"
#include "aspect_ratio.h"
#include "cache_device.h"
#include "cheat_menu_input.h"
#include "cheat_menu_key.h"
#include "cheat_menu_overlay.h"
#include "frame_counter.h"
#include "frame_rate.h"
#include "game_files.h"
#include "glyph_overlay.h"
#include "guest_fault_log.h"
#include "hang_watchdog.h"
#include "internal_render_scale.h"
#include "language_setting.h"
#include "launcher.h"
#include "log_filter.h"
#include "log_path.h"
#include "main_menu_quit.h"
#include "mouse_look.h"
#include "orb_map.h"
#include "platform.h"
#include "session_log.h"
#include "settings.h"
#include "skip_intro.h"
#include "user_data.h"
#include "window_host.h"

class CrackdownApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    crackdown::SetApplicationId();
    return std::unique_ptr<CrackdownApp>(new CrackdownApp(ctx, "crackdown", PPCImageConfig));
  }

 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    shipped_config_ = paths.config_path;
    if (const auto inherited = crackdown::TakeInheritedSessionLog()) {
      rex::cvar::SetFlagFromCommandLine("log_file", crackdown::Utf8(*inherited));
      crackdown::InstallCrashReports();
    }
    const std::filesystem::path port_folder = crackdown::PortFolder();
    if (paths.game_data_root.empty()) {
      paths.game_data_root = port_folder / "game";
    }
    ChooseUserData(paths, port_folder);
    // ReXGlue writes its logs beside the executable, which is read-only inside an AppImage.
    const bool appimage = port_folder != rex::filesystem::GetExecutableFolder();
    if (appimage && rex::cvar::GetFlagByName("log_file").empty()) {
      const std::filesystem::path logs = crackdown::LogsFolder(port_folder, paths.user_data_root, {}, appimage);
      if (const std::filesystem::path file = crackdown::NextLogFile(logs, "crackdown"); !file.empty()) {
        rex::cvar::SetFlagByName("log_file", crackdown::Utf8(file));
      }
    }
  }

  bool SetupPresentation() override {
    if (!crackdown::GraphicalSessionAvailable()) {
      return false;
    }
    if (rex::ReXApp::SetupPresentation()) {
      return true;
    }
    REXLOG_WARN("Graphics unavailable; showing the graphics error");
    crackdown::RunLauncherWithoutGraphics(DescribeGameFiles());
    return false;
  }

  void OnPostInitLogging() override {
    crackdown::InstallCrashReports();
    if (crackdown::RecoverInterruptedInstall(game_data_root())) {
      REXLOG_WARN("Restored the game files after an interrupted install");
    }
    const crackdown::UserDataMessage message = crackdown::DescribeUserData(user_data_);
    if (message.warning) {
      REXLOG_WARN("{}", message.text);
    } else {
      REXLOG_INFO("{}", message.text);
    }
    crackdown::LoadPlayerSettings(crackdown::PlayerSettingsPath(user_data_root()));
    crackdown::ApplyDetailedLogging();
    crackdown::InstallSessionLogCleanup();
    REXLOG_INFO("Crackdown port {} ({}, {}, ReXGlue SDK {})", CRACKDOWN_PORT_VERSION,
                crackdown::ArchitectureName(), crackdown::GraphicsBackendName(), REXGLUE_VERSION_STRING);
    crackdown::ApplyGraphicsAdapter();
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    crackdown::LoadLauncherFonts(atlas, window() ? float(window()->GetDpi()) / float(window()->GetMediumDpi()) : 1.0f);
  }

  void OnDpiScaleChanged(float scale) override {
    crackdown::UpdateLauncherFonts(imgui_drawer(), immediate_drawer(), scale);
  }

  void OnPreSetup(rex::RuntimeConfig&) override {
    game_settings_.Capture(crackdown::PlayerSettingsPath(user_data_root()), shipped_config_);
    show_launcher_ = crackdown::ShouldShowLauncher(game_data_root());
    if (show_launcher_) {
      crackdown::PrepareLauncherWindow();
    }
    game_settings_.MarkWindowCreated();
    crackdown::FitWindowToDisplay();
  }

  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
                                                 std::function<void(rex::PathConfig)> resume) override {
    if (window()) {
      window()->SetTitle(kWindowTitle);
    }
    crackdown::ApplyWindowIcon();
    game_settings_.CaptureLateRegistered();
    if (!show_launcher_ && crackdown::ShiftHeldAfterWindowOpened()) {
      show_launcher_ = true;
    }
    if (!show_launcher_) {
      crackdown::ApplyAspectRatio();
      crackdown::ApplyInternalRenderPath();
      crackdown::ApplyMultisampling();
      return defaults;
    }
    if (!imgui_drawer()) {
      game_settings_.Apply();
      return defaults;
    }
    if (crackdown::CheckGameFiles(game_data_root()).state == crackdown::GameFiles::kReady) {
      show_launcher_after_runtime_ = true;
      crackdown::ApplyAspectRatio();
      crackdown::ApplyInternalRenderPath();
      crackdown::ApplyMultisampling();
      return defaults;
    }
    REXLOG_INFO("Launcher shown");
    launcher_open_ = true;
    crackdown::ShowLauncher(imgui_drawer(),
                            {defaults.game_data_root, defaults.user_data_root,
                             crackdown::PlayerSettingsPath(defaults.user_data_root), LogsDir(defaults.user_data_root),
                             user_data_.outcome},
                            game_settings_, {nullptr, immediate_drawer(), nullptr},
                            MakeLauncherCallbacks([resume, defaults] { resume(defaults); }));
    return std::nullopt;
  }

  // With game files installed, the launcher opens after the runtime to read its achievement manager.
  void LaunchModule() override {
    if (!show_launcher_after_runtime_) {
      ReXApp::LaunchModule();
      return;
    }
    show_launcher_after_runtime_ = false;
    REXLOG_INFO("Launcher shown after the runtime");
    launcher_open_ = true;
    SetCursorOverlayOpen(true);
    crackdown::LauncherCallbacks callbacks = MakeLauncherCallbacks([this] { ReXApp::LaunchModule(); });
    // Settings the runtime reads at startup take effect only in a new process.
    callbacks.window_changed = [this] { return game_settings_.WindowChanged() || game_settings_.RuntimeChanged(); };
    crackdown::ShowLauncher(imgui_drawer(),
                            {game_data_root(), user_data_root(), crackdown::PlayerSettingsPath(user_data_root()),
                             LogsDir(user_data_root()), user_data_.outcome},
                            game_settings_, {runtime(), immediate_drawer(), &achievements()}, std::move(callbacks));
  }

  // F7 toggles the overlay owned here; ReXApp's own list has no B, X or controller handling.
  std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override {
    if (launcher_open_ || cheat_menu_) {
      return nullptr;
    }
    if (achievements_) {
      CloseAchievements();
    } else {
      OpenAchievements();
    }
    return nullptr;
  }

  void OnPostSetup() override {
    const crackdown::WindowSize picture =
        crackdown::RenderPicture(crackdown::ParseAspectRatio(REXCVAR_GET(crackdown_aspect_ratio)));
    REXLOG_INFO("{}", crackdown::DescribeInternalRenderResolution(
                          picture, std::max(1, std::atoi(rex::cvar::GetFlagByName("resolution_scale").c_str())),
                          std::atoi(rex::cvar::GetFlagByName("internal_render_scale").c_str())));
#if defined(_WIN32)
    REXLOG_INFO("Scheduling: {}", trg::TuneProcessScheduling());
#endif
    if (window()) {
      const uint32_t width = window()->GetActualLogicalWidth();
      const uint32_t height = window()->GetActualLogicalHeight();
      if (width != 0 && height != 0) {
        REXLOG_INFO("Window: {}x{} at {} DPI, {}", width, height, window()->GetDpi(),
                    window()->IsFullscreen() ? "borderless fullscreen" : "windowed");
      }
    }
    if (runtime()) {
      MountCacheDevice(runtime()->file_system(), user_data_root());
      crackdown::InstallCrashDiagnostics(runtime()->virtual_membase());
      crackdown::InstallGlyphOverlay(runtime()->file_system(), game_data_root(), user_data_root(),
                                     crackdown::LanguageFor(REXCVAR_GET(crackdown_language)).key);
      // Crackdown has no local split screen and polls user 0 (XUSER_INDEX_ANY is pinned there), so every pad drives
      // user 0 instead of the default one-pad-per-user slots.
      if (auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system())) {
        input->SetDeviceAssignment(std::make_unique<rex::input::SharedAssignment>());
      }
    }
    if (window()) {
      AttachWindowHost(
          window(), app_context(),
          [this] {
            rex::ui::ImGuiDrawer* drawer = imgui_drawer();
            return drawer && drawer->GetIO().WantCaptureMouse;
          },
          [this] { ToggleWindowMode(); });
      SetCursorOverlayOpen(launcher_open_);
      crackdown::AttachFrameRate(window(), app_context());
      crackdown::CheckWindowDisplay(window()->GetNativeWindowHandle(), REXCVAR_GET(monitor));
      crackdown::StartHangWatchdog(window()->GetNativeWindowHandle(), LogsDir(user_data_root()));
      crackdown::mouse_look::Install(window());
    }
    crackdown::mouse_look::Configure();
    if (imgui_drawer()) {
      crackdown::CreateFrameCounter(imgui_drawer());
      crackdown::orb_map::CreateOverlay(imgui_drawer());
      CreateCursorSync(imgui_drawer());
    }
    game_settings_.MarkRuntimeBuilt();
    crackdown::SetAchievementsCloseRequest(
        [this] { app_context().CallInUIThreadDeferred([this] { CloseAchievements(); }); });
    crackdown::main_menu_quit::SetQuitHandler([this] { RequestHardExitFromUIThread(); });
    crackdown::main_menu_quit::Install();
    InstallSkipIntro();
    if (window()) {
      cheat_menu_key_ = std::make_unique<crackdown::CheatMenuKey>(
          window(), [this] { app_context().CallInUIThreadDeferred([this] { ToggleCheatMenu(); }); });
    }
  }

  void OnFileDrop(rex::ui::FileDropEvent& e) override { crackdown::LauncherFileDropped(e.filename()); }

  void OnShutdown() override {
    CloseAchievements();
    cheat_menu_key_.reset();
    CloseCheatMenu();
    crackdown::mouse_look::Uninstall();
    DetachWindowHost();
    // ReXApp destroys the window before the runtime; the input drivers must detach while it still exists.
    if (auto* input = runtime() ? runtime()->input_system() : nullptr) {
      input->Shutdown();
    }
    crackdown::DetachFrameRate();
    crackdown::FinalizeSessionLog();
  }

  // The hard-exit path must not wait for guest threads: the OS reclaims the process. Finalizing here is the
  // only chance to apply the logging policy before the hard exit.
  void OnClosing(rex::ui::UIEvent& e) override {
    (void)e;
    HardExitFromUIThread();
  }

 private:
  void RequestHardExitFromUIThread() {
    app_context().CallInUIThreadDeferred([this] { HardExitFromUIThread(); });
  }

  void HardExitFromUIThread() {
    DetachWindowHost();
    REXLOG_INFO("Window closing, hard-exiting process.");
    crackdown::FinalizeSessionLog();
    rex::FlushLogging();
    std::_Exit(0);
  }

  crackdown::LauncherCallbacks MakeLauncherCallbacks(std::function<void()> start) {
    crackdown::LauncherCallbacks callbacks;
    callbacks.play = [this, start] {
      launcher_open_ = false;
      SetCursorOverlayOpen(false);
      app_context().CallInUIThreadDeferred([this, start] {
        game_settings_.Apply();
        crackdown::ApplyDetailedLogging();
        game_settings_.ApplyDisplayMode();
        start();
      });
    };
    callbacks.window_changed = [this] { return game_settings_.WindowChanged(); };
    callbacks.restart = [this] {
      if (!crackdown::RelaunchSelf(L"--crackdown_skip_launcher=true")) {
        return false;
      }
      app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
      return true;
    };
    callbacks.quit = [this] {
      launcher_open_ = false;
      SetCursorOverlayOpen(false);
      app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
    };
    callbacks.window_handle = [this] { return window() ? window()->GetNativeWindowHandle() : nullptr; };
    callbacks.window_fullscreen = [this] { return window() && window()->IsFullscreen(); };
    callbacks.set_window_fullscreen = [this](bool fullscreen) {
      if (window()) {
        window()->SetFullscreen(fullscreen);
      }
    };
    return callbacks;
  }

  void OpenAchievements() {
    if (achievements_ || cheat_menu_ || !runtime() || !runtime()->kernel_state() || !imgui_drawer() ||
        !immediate_drawer()) {
      return;
    }
    achievements_ = std::make_unique<crackdown::AchievementsOverlay>(
        imgui_drawer(), immediate_drawer(), runtime(), &achievements(),
        crackdown::AchievementsOverlay::Controls{
            .request_close = [this] { app_context().CallInUIThreadDeferred([this] { CloseAchievements(); }); },
            .scroll_axis = [] { return crackdown::AchievementsScrollAxis(); },
        });
    crackdown::SetAchievementsModal(true);
    SetCursorOverlayOpen(true);
  }

  void CloseAchievements() {
    if (!achievements_) {
      return;
    }
    achievements_.reset();
    crackdown::SetAchievementsModal(false);
    SetCursorOverlayOpen(false);
  }

  void ToggleCheatMenu() {
    if (cheat_menu_) {
      cheat_menu_->RequestClose();
    } else {
      OpenCheatMenu();
    }
  }

  void OpenCheatMenu() {
    if (launcher_open_ || achievements_ || !runtime() || !imgui_drawer()) {
      return;
    }
    crackdown::cheats::SetMenuInputOpen(true);
    cheat_menu_ = std::make_unique<crackdown::CheatMenuOverlay>(
        imgui_drawer(), crackdown::CheatMenuOverlay::Controls{
                            .finished = [this] { app_context().CallInUIThreadDeferred([this] { CloseCheatMenu(); }); },
                        });
    SetCursorOverlayOpen(true);
  }

  void CloseCheatMenu() {
    if (!cheat_menu_) {
      return;
    }
    cheat_menu_.reset();
    crackdown::cheats::SetMenuInputOpen(false);
    SetCursorOverlayOpen(false);
  }

  std::filesystem::path LogsDir(const std::filesystem::path& user_data_root) const {
    const std::filesystem::path port_folder = crackdown::PortFolder();
    return crackdown::LogsFolder(port_folder, user_data_root, crackdown::ActiveLogFile(),
                                 port_folder != rex::filesystem::GetExecutableFolder());
  }

  crackdown::GraphicsFallbackInfo DescribeGameFiles() const {
    const std::string folder = crackdown::Utf8(game_data_root());
    switch (crackdown::CheckGameFiles(game_data_root()).state) {
      case crackdown::GameFiles::kReady:
        return {folder, "Ready", true};
      case crackdown::GameFiles::kUnsupported:
        return {folder, "Unsupported version", false};
      default:
        return {folder, "Setup required", false};
    }
  }

  // F11 changes the window at once and keeps the choice as the player's Display mode for the next start.
  void ToggleWindowMode() {
    const bool fullscreen = !window()->IsFullscreen();
    const std::string value = fullscreen ? "true" : "false";
    window()->SetFullscreen(fullscreen);
    rex::cvar::SetFlagByName(crackdown::kFullscreenKey, value);
    game_settings_.SaveNow(crackdown::kFullscreenKey, value);
  }

  // The cache folder ReXGlue derived from the system root follows the user data root unless it was set explicitly.
  void ChooseUserData(rex::PathConfig& paths, const std::filesystem::path& port_folder) {
    user_data_ = crackdown::SelectUserData(
        {port_folder, REXCVAR_GET(user_data_root).empty() ? std::filesystem::path() : paths.user_data_root});
    if (user_data_.outcome == crackdown::UserDataOutcome::kNotWritable) {
      crackdown::ShowFatalMessage(crackdown::UserDataNotWritableText(user_data_));
      std::_Exit(1);
    }
    if (user_data_.root == paths.user_data_root) {
      return;
    }
    paths.user_data_root = user_data_.root;
    if (REXCVAR_GET(cache_root).empty()) {
      paths.cache_root = user_data_.root / "cache";
    }
  }

  crackdown::GameSettings game_settings_;
  std::filesystem::path shipped_config_;
  crackdown::UserDataSelection user_data_;
  std::unique_ptr<crackdown::AchievementsOverlay> achievements_;
  std::unique_ptr<crackdown::CheatMenuOverlay> cheat_menu_;
  std::unique_ptr<crackdown::CheatMenuKey> cheat_menu_key_;
  bool show_launcher_ = false;
  bool show_launcher_after_runtime_ = false;
  bool launcher_open_ = false;
};
