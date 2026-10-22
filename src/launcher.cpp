#include "launcher.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/system/achievement_manager.h>
#include <rex/ui/image_decode.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/overlay/achievement_icon_cache.h>
#include <rex/version.h>

#include "trg/launcher.h"
#include "trg/platform.h"

#include "agency_accent.h"
#include "aspect_ratio.h"
#include "cheats.h"
#include "dpi.h"
#include "frame_timing.h"
#include "game_files.h"
#include "graphics_adapters.h"
#include "internal_render_scale.h"
#include "keybinds.h"
#include "language_setting.h"
#include "launcher_achievements.h"
#include "launcher_achievements_page.h"
#include "launcher_assets.h"
#include "launcher_pages.h"
#include "launcher_quit.h"
#include "log_filter.h"
#include "mouse_look.h"
#include "platform.h"
#include "presentation_setting.h"
#include "settings.h"
#include "update_config.h"
#include "updater/update_ui.h"

namespace crackdown {
namespace {

// Logical pixels.
constexpr std::pair<int, int> kLauncherWindowSize = {1280, 720};
// Share of the primary display's work area the launcher may take.
constexpr double kLauncherMaxShare = 0.9;
// How long the saved message stays on screen before the game starts.
constexpr double kPlayFeedbackSeconds = 0.4;

constexpr ImGuiConfigFlags kNavFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

// The Agency's HUD blue, over the Midnight background.
const ImVec4 kAccent = ImVec4(kAccentRed, kAccentGreen, kAccentBlue, 1.0f);
const ImVec4 kDeepBlue = ImVec4(0.07f, 0.18f, 0.35f, 1.0f);

constexpr const char* kAuthorUrl = "https://github.com/saulob";
constexpr const char* kRepositoryUrl = "https://github.com/saulob/crackdown-xbox360-recompiled";

trg::Theme LauncherTheme() {
  trg::Theme theme = trg::Theme::Midnight().WithAccent(kAccent);
  theme.good = kDeepBlue;
  theme.good_dot = kAccent;
  return theme;
}

constexpr float kHeaderArtAspect = 1600.0f / 180.0f;

trg::HeaderPainter LauncherHeader(ImTextureID art, float art_aspect) {
  return [art, art_aspect](const trg::HeaderContext& c) {
    c.draw->AddRectFilledMultiColor(c.min, c.max, IM_COL32(14, 18, 30, 255), IM_COL32(12, 42, 68, 255),
                                    IM_COL32(8, 24, 40, 255), IM_COL32(8, 10, 18, 255));
    if (art == ImTextureID{}) {
      return;
    }
    const float height = c.max.y - c.min.y;
    const float width = height * art_aspect;
    const ImVec2 top_left(c.max.x - width, c.min.y);
    c.draw->AddImage(ImTextureRef(art), top_left, c.max);
  };
}

std::unique_ptr<rex::ui::ImmediateTexture> LoadHeaderArt(rex::ui::ImmediateDrawer* drawer) {
  const std::filesystem::path path = LauncherHeaderArtPath(rex::filesystem::GetExecutableFolder());
  if (!drawer) {
    REXLOG_WARN("Launcher header art unavailable without an immediate drawer: {}", Utf8(path));
    return nullptr;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    REXLOG_WARN("Launcher header art not found: {}", Utf8(path));
    return nullptr;
  }
  const std::vector<uint8_t> encoded((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  int width = 0;
  int height = 0;
  const std::vector<uint8_t> pixels = rex::ui::DecodeImageRGBA(encoded.data(), encoded.size(), width, height);
  if (pixels.empty() || width <= 0 || height <= 0) {
    REXLOG_WARN("Launcher header art could not be decoded: {}", Utf8(path));
    return nullptr;
  }
  return drawer->CreateTexture(uint32_t(width), uint32_t(height), rex::ui::ImmediateTextureFilter::kLinear, false,
                               pixels.data());
}

trg::Fonts g_fonts;
float g_font_density = 1.0f;

GameFiles CheckAndLogGameFiles(const std::filesystem::path& game_dir) {
  GameFiles files = CheckGameFiles(game_dir);
  if (files.state != GameFiles::kReady && files.state != GameFiles::kMissing) {
    REXLOG_WARN("Game files in {}: {}", Utf8(game_dir), files.detail);
  }
  return files;
}

// Written by the install job, read once the Task has finished.
struct InstallOutcome {
  InstallResult result;
  GameFiles files;
};

std::string Size(int width, int height) {
  return std::to_string(width) + " \xC3\x97 " + std::to_string(height);
}

std::string DisplayUrl(const char* url) {
  const std::string text = url;
  const size_t scheme = text.find("://");
  return scheme == std::string::npos ? text : text.substr(scheme + 3);
}

std::vector<trg::Option> RenderResolutionOptions(AspectRatio aspect) {
  const WindowSize picture = RenderPicture(aspect);
  std::vector<trg::Option> options;
  for (int scale = 1; scale <= 4; ++scale) {
    std::string label = scale == 1 ? "Original" : std::to_string(scale) + "x";
    label += " \xE2\x80\x94 " + std::to_string(picture.width * scale) + "x" + std::to_string(picture.height * scale);
    if (scale == 2) {
      label += " \xE2\x80\x94 Recommended for 1440p";
    }
    options.push_back({std::to_string(scale), label});
  }
  return options;
}

std::vector<trg::Option> AspectRatioOptions() {
  std::vector<trg::Option> options;
  for (const AspectRatioInfo& info : kAspectRatios) {
    options.push_back({info.key, info.label});
  }
  return options;
}

std::vector<AchievementEntry> LoadAchievementEntries(const rex::system::AchievementManager& manager) {
  std::vector<AchievementEntry> entries;
  for (rex::system::AchievementInfo& info : manager.ListAchievements()) {
    const bool unlocked = manager.IsUnlocked(info.id);
    entries.push_back({std::move(info), unlocked});
  }
  return entries;
}

class LauncherDialog;
LauncherDialog* g_dialog = nullptr;

class LauncherDialog final : public rex::ui::ImGuiDialog {
 public:
  LauncherDialog(rex::ui::ImGuiDrawer* drawer, LauncherPaths paths, GameSettings& settings, LauncherGameContext game,
                 LauncherCallbacks callbacks)
      : ImGuiDialog(drawer),
        paths_(std::move(paths)),
        cb_(std::move(callbacks)),
        settings_(settings),
        game_(game),
        game_files_(CheckAndLogGameFiles(paths_.game_dir).state),
        monitors_(ListMonitors()),
        graphics_adapters_(ListGraphicsAdapters()) {
    g_dialog = this;
    if (game_.achievements) {
      entries_ = LoadAchievementEntries(*game_.achievements);
    }
    if (game_.runtime && game_.immediate_drawer) {
      icons_ = std::make_unique<rex::ui::AchievementIconCache>(game_.immediate_drawer, game_.runtime);
    }
    if (SelectedAspectRatio() != AspectRatio::k16x9) {
      SelectWindowSizeForAspect();
    }
    header_art_ = LoadHeaderArt(game_.immediate_drawer);
    launcher_ = std::make_unique<trg::Launcher>(MakeConfig());
    updater_ = std::make_unique<trg::update::UpdateController>(MakeUpdateConfig(PortFolder()));
    updater_->StartCheck();
    if (WindowFullscreen()) {
      restore_when_windowed_ = true;
      RequestFullscreen(false);
    } else {
      StartInSavedState();
    }
  }

  ~LauncherDialog() override {
    if (g_dialog == this) {
      g_dialog = nullptr;
    }
  }

  void FileDropped(const std::filesystem::path& path) {
    if (starting_ || install_.running()) {
      return;
    }
    StartInstall(path);
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (done_) {
      return;
    }
    PollInstall();
    PollUpdate();
    TrackPlacement();
    if (!nav_taken_) {
      saved_nav_flags_ = io.ConfigFlags & kNavFlags;
      saved_has_gamepad_ = io.BackendFlags & ImGuiBackendFlags_HasGamepad;
      io.ConfigFlags |= kNavFlags;
      nav_taken_ = true;
    }
    gamepad_.Update(io);
    if (!starting_ && ImGui::IsKeyPressed(ImGuiKey_F11, false) && !fullscreen_gate_.Pending()) {
      ToggleLauncherFullscreen();
    }
    if (starting_ && ImGui::GetTime() - play_started_ >= kPlayFeedbackSeconds && !LeavingFullscreen()) {
      StartGame();
      return;
    }
    if (starting_) {
      // TRG's hotkeys read the same key state, so release it before Frame() runs.
      io.ClearInputKeys();
      io.ClearInputMouse();
    }
    ImGui::BeginDisabled(starting_);
    const trg::Result result = launcher_->Frame();
    ImGui::EndDisabled();
    if (result == trg::Result::kNone && !starting_) {
      HandleUpdateAction(
          trg::update::DrawUpdateBanner(updater_->snapshot(), launcher_->config().theme, g_fonts));
    }
    if (starting_) {
      DrawStartingMessage();
    }
    switch (result) {
      case trg::Result::kPlay:
        RequestPlay();
        break;
      case trg::Result::kQuit:
        if (!starting_ && crackdown::LauncherQuitAllowed(ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
          Finish(cb_.quit);
        }
        break;
      case trg::Result::kNone:
        break;
    }
  }

  // Every PLAY (mouse, Enter, Xbox Start) ends here. The one save happens here; StartGame never saves.
  void RequestPlay() {
    if (starting_ || install_.running() || game_files_ != GameFiles::kReady) {
      return;
    }
    if (!launcher_->Save()) {
      return;
    }
    starting_ = true;
    play_started_ = ImGui::GetTime();
    launcher_->SetStatus("", 0.0);
  }

  void StartGame() {
    if (cb_.window_changed && cb_.window_changed()) {
      if (cb_.restart && cb_.restart()) {
        Finish(nullptr);
        return;
      }
      starting_ = false;
      launcher_->SetStatus("Could not start the game in a new window. Press Play to try again.", 8);
      return;
    }
    RestoreStartupWindow();
    Finish(cb_.play);
  }

  // Drawn outside the disabled frame so the message keeps full brightness.
  void DrawStartingMessage() {
    constexpr const char* kText = "Settings saved. Starting game...";
    ImFont* font = g_fonts.semibold ? g_fonts.semibold : ImGui::GetFont();
    const float size = g_fonts.size * 1.25f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.0f, kText);
    const ImVec2 pos(vp->Pos.x + (vp->Size.x - text_size.x) * 0.5f, vp->Pos.y + (vp->Size.y - text_size.y) * 0.5f);
    ImGui::GetForegroundDrawList()->AddText(font, size, pos, ImGui::GetColorU32(kAccent), kText);
  }

  // Runs inside the last frame; the in-game overlays share this ImGui context.
  void OnClose() override {
    if (updater_) {
      updater_->Cancel();
      if (updater_->Busy()) {
        updater_.release();
      }
    }
    ImGuiIO& io = GetIO();
    gamepad_.Release(io);
    io.ConfigFlags = (io.ConfigFlags & ~kNavFlags) | saved_nav_flags_;
    io.BackendFlags = (io.BackendFlags & ~ImGuiBackendFlags_HasGamepad) | saved_has_gamepad_;
  }

 private:
  void Finish(std::function<void()> action) {
    done_ = true;
    if (updater_) {
      updater_->Cancel();
      if (updater_->Busy()) {
        // Do not join a network call on the UI thread when the launcher leaves.
        updater_.release();
      }
    }
    if (!skip_finish_save_) {
      SavePlacement(placement_.Flush());
    }
    Close();
    if (action) {
      action();
    }
  }

  void* WindowHandle() const { return cb_.window_handle ? cb_.window_handle() : nullptr; }

  bool WindowFullscreen() const { return cb_.window_fullscreen && cb_.window_fullscreen(); }

  void StartInSavedState() {
    RestorePlacement();
    if (fullscreen_pref_.wanted()) {
      RequestFullscreen(true);
    }
  }

  void ToggleLauncherFullscreen() {
    const bool fullscreen = !WindowFullscreen();
    RequestFullscreen(fullscreen);
    fullscreen_pref_.UserSet(fullscreen);
    if (!settings_.SaveNow(LauncherFullscreenValues(fullscreen))) {
      REXLOG_WARN("Could not save the launcher fullscreen state");
    }
  }

  void RequestFullscreen(bool fullscreen) {
    fullscreen_gate_.Request(fullscreen);
    if (cb_.set_window_fullscreen) {
      cb_.set_window_fullscreen(fullscreen);
    }
  }

  // The game gets the window out of the launcher's temporary fullscreen.
  bool LeavingFullscreen() {
    if (!WindowFullscreen() || leave_gave_up_) {
      return false;
    }
    if (!leave_requested_) {
      leave_requested_ = true;
      leave_started_ = ImGui::GetTime();
      RequestFullscreen(false);
    }
    leave_gave_up_ = ImGui::GetTime() - leave_started_ >= FullscreenGate::kTimeoutSeconds;
    return !leave_gave_up_;
  }

  void RestorePlacement() {
    if (const auto placed = TakePreShowResult()) {
      startup_ = placed->startup;
      placement_ = PlacementTracker(placed->applied);
      return;
    }
    void* window = WindowHandle();
    const auto state = QueryWindowState(window);
    if (!state || WindowFullscreen()) {
      return;
    }
    const WindowPlacement saved = SavedLauncherPlacement();
    const bool restorable = PlacementIsVisible(saved, WorkAreas());
    startup_ = WindowPlacement{state->rect, state->maximized};
    placement_ = PlacementTracker(restorable ? std::optional<WindowPlacement>(saved) : std::nullopt);
    if (restorable) {
      ApplyWindowPlacement(window, saved);
    }
  }

  void TrackPlacement() {
    const double now = ImGui::GetTime();
    const FullscreenGate::Step step = fullscreen_gate_.Update(WindowFullscreen(), now);
    if (restore_when_windowed_) {
      if (!WindowFullscreen() && !fullscreen_gate_.Pending()) {
        restore_when_windowed_ = false;
        StartInSavedState();
      }
      return;
    }
    if (!startup_) {
      return;
    }
    if (step.restore) {
      if (const auto current = placement_.Current()) {
        ApplyWindowPlacement(WindowHandle(), *current);
      }
    }
    if (!step.observe) {
      return;
    }
    if (const auto state = QueryWindowState(WindowHandle())) {
      placement_.Observe(*state, WindowFullscreen(), now);
      SavePlacement(placement_.TakeSettled(now));
    }
  }

  void SavePlacement(const std::optional<WindowPlacement>& placement) {
    if (placement && !settings_.SaveNow(PlacementValues(*placement))) {
      REXLOG_WARN("Could not save the launcher window placement");
    }
  }

  // The game keeps the window it was created with, not the launcher's saved placement.
  void RestoreStartupWindow() {
    if (startup_ && !WindowFullscreen()) {
      ApplyWindowPlacement(WindowHandle(), *startup_);
    }
  }

  trg::LauncherConfig MakeConfig() {
    trg::LauncherConfig c;
    c.settings = &settings_;
    // PLAY saves in RequestPlay, so a failed save keeps the launcher open.
    c.save_on_play = false;
    c.theme = LauncherTheme();
    c.note = "Play applies the changes. Save keeps them without starting.";
    c.hints = "Enter  Play     Alt+F4  Quit     Ctrl+S  Save";
    c.fonts = g_fonts;
    c.branding.title = "CRACKDOWN";
    c.branding.subtitle_segments = {
        {"PC PORT   \xC2\xB7   ", ""},
        {"v" CRACKDOWN_PORT_VERSION "   \xC2\xB7   ", ""},
        {"BY SAULOB", kAuthorUrl},
    };
    const float art_aspect =
        header_art_ ? float(header_art_->width) / float(header_art_->height) : kHeaderArtAspect;
    c.branding.background = LauncherHeader(header_art_ ? reinterpret_cast<ImTextureID>(header_art_.get())
                                                       : ImTextureID{},
                                           art_aspect);
    for (const LauncherPage page : kLauncherPageOrder) {
      c.pages.push_back(PageFor(page));
    }
    c.can_play = [this] {
      if (install_.running()) {
        return trg::PlayCheck{false, "Wait for the install to finish.", 0};
      }
      if (game_files_ == GameFiles::kReady) {
        return trg::PlayCheck{};
      }
      return trg::PlayCheck{false, "The game files are not ready: see the Play page.", 0};
    };
    c.on_file_drop = [this](const std::string& path) { FileDropped(PathFromUtf8(path)); };
    return c;
  }

  trg::Page PageFor(LauncherPage page) {
    switch (page) {
      case LauncherPage::kPlay:
        return {"Play", "Check the game files and start playing.", [this](trg::Ui& ui) { PagePlay(ui); },
                [this] { return game_files_ != GameFiles::kReady; }};
      case LauncherPage::kDisplay:
        return {"Display", "Window, monitor and how the picture fits your screen.",
                [this](trg::Ui& ui) { PageDisplay(ui); }, nullptr};
      case LauncherPage::kGraphics:
        return {"Graphics", "Graphics card, render resolution, anti-aliasing and texture filtering.",
                [this](trg::Ui& ui) { PageGraphics(ui); }, nullptr};
      case LauncherPage::kGameplay:
        return {"Gameplay", "Frame rate, and what the window shows while you play.",
                [](trg::Ui& ui) { PageGameplay(ui); }, nullptr};
      case LauncherPage::kCheats:
        return {"Cheats", "Optional gameplay modifiers", [](trg::Ui& ui) { PageCheats(ui); }, nullptr};
      case LauncherPage::kControls:
        return {"Controls", "Controller, keyboard and mouse.", [](trg::Ui& ui) { PageControls(ui); }, nullptr};
      case LauncherPage::kAchievements:
        return {"Achievements", "Your achievements on this PC.", [this](trg::Ui& ui) { PageAchievements(ui); },
                nullptr};
      case LauncherPage::kAbout:
        return {"About", "About this port, and where your files and settings live.",
                [this](trg::Ui& ui) { PageAbout(ui); }, nullptr};
    }
    return {};
  }

  void PageAchievements(trg::Ui& ui) {
    if (!game_.achievements) {
      ui.Paragraph("Install the game to see its achievements.");
      return;
    }
    if (entries_.empty()) {
      ui.Paragraph("This game lists no achievements.");
      return;
    }
    DrawAchievementsPage(ui, entries_, icons_.get());
  }

  void PagePlay(trg::Ui& ui) {
    const std::string dir = Utf8(paths_.game_dir);
    if (install_.running()) {
      ui.StatusCard(trg::Status::kBusy, "Installing game files", nullptr, std::max(0.0f, install_.fraction()));
    } else {
      switch (game_files_) {
        case GameFiles::kReady:
          ui.StatusCard(trg::Status::kReady, "Ready to play", dir.c_str());
          break;
        case GameFiles::kUnsupported:
          ui.StatusCard(trg::Status::kAttention, "Different version of the game",
                        DescribeInstall(InstallStatus::kWrongVersion).text.c_str());
          break;
        case GameFiles::kIncomplete:
          ui.StatusCard(trg::Status::kAttention, "Game files incomplete",
                        DescribeInstall(InstallStatus::kIncomplete).text.c_str());
          break;
        case GameFiles::kMissing:
          ui.StatusCard(trg::Status::kAttention, "Game files needed",
                        "This port requires your own copy of the Crackdown disc.");
          break;
      }
    }
    if (!install_failure_.empty()) {
      DrawFailure(ui, install_failure_, install_failure_text_);
    }
    if (paths_.game_dir.empty()) {
      ui.Help("No game folder was given: start the game with --game_data_root.");
    } else {
      ui.Row("Game files",
             "Title ID: 4D5307DC. No game files are included with this port: the installer copies them (about 6 GB) "
             "from your own Crackdown disc image or from a folder with the disc's contents. You can also drop one "
             "on this window.");
      if (!ui.TaskProgress(install_)) {
        const bool ready = game_files_ == GameFiles::kReady;
        if (ready ? ImGui::Button("Reinstall from disc image...", ImVec2(-FLT_MIN, 0))
                  : ui.AccentButton("Install from disc image...")) {
          BrowseAndInstall(false);
        }
        if (ImGui::Button("Install from folder...", ImVec2(-FLT_MIN, 0))) {
          BrowseAndInstall(true);
        }
      }
    }
    ui.LauncherVisibilityRow("crackdown_launcher");
    ui.EndRows();
  }

  static void DrawFailure(trg::Ui& ui, const std::string& title, const std::string& text) {
    ui.Spacer(4);
    ImGui::PushStyleColor(ImGuiCol_Text, ui.theme().warn_dot);
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopStyleColor();
    ui.Help(text.c_str());
    ui.Spacer(4);
  }

  void BrowseAndInstall(bool folder) {
    PollInstall();
    if (install_.running()) {
      return;
    }
    const std::string picked =
        folder ? trg::BrowseForFolder("Select the folder with the Crackdown disc contents")
               : trg::BrowseForFile("Select your Crackdown disc image", {{"Xbox 360 disc image", "*.iso"}});
    if (!picked.empty()) {
      StartInstall(PathFromUtf8(picked));
    }
  }

  void StartInstall(const std::filesystem::path& source) {
    install_failure_.clear();
    install_failure_text_.clear();
    outcome_ = std::make_shared<InstallOutcome>();
    const bool started = install_.Start(
        "Reading the game files...",
        [outcome = outcome_, source, game_dir = paths_.game_dir](trg::Task& task) {
          InstallHooks hooks;
          hooks.progress = [&task](const InstallProgress& progress) {
            task.SetLabel(progress.phase);
            task.Progress(static_cast<long long>(progress.done_bytes), static_cast<long long>(progress.total_bytes));
            return !task.cancelled();
          };
          outcome->result = InstallGame(source, game_dir, hooks);
          if (outcome->result.ok()) {
            outcome->files = CheckGameFiles(game_dir);
          }
          return outcome->result.ok() ? std::string() : DescribeInstall(outcome->result.status).title;
        });
    if (started) {
      REXLOG_INFO("Installing game files from {}", Utf8(source));
    } else {
      outcome_.reset();
    }
  }

  void PollUpdate() {
    if (updater_) {
      updater_->Poll();
    }
  }

  void HandleUpdateAction(trg::update::UpdateUiAction action) {
    switch (action) {
      case trg::update::UpdateUiAction::kDownload:
        updater_->StartDownload();
        break;
      case trg::update::UpdateUiAction::kRetry:
        if (updater_->RetryNeedsInstall()) {
          if (install_.running() || starting_) {
            launcher_->SetStatus("Wait for the current install to finish.", 5.0);
            break;
          }
          if (!launcher_->Save()) {
            break;
          }
          SavePlacement(placement_.Flush());
          if (updater_->Retry()) {
            skip_finish_save_ = true;
            Finish(cb_.quit);
          }
        } else {
          updater_->Retry();
        }
        break;
      case trg::update::UpdateUiAction::kRestart:
        if (install_.running() || starting_) {
          launcher_->SetStatus("Wait for the current install to finish.", 5.0);
          break;
        }
        if (!launcher_->Save()) {
          break;
        }
        SavePlacement(placement_.Flush());
        if (updater_->BeginInstall()) {
          skip_finish_save_ = true;
          Finish(cb_.quit);
        }
        break;
      case trg::update::UpdateUiAction::kNone:
        break;
    }
  }

  // Runs every frame so a finished install is picked up whichever page is open.
  void PollInstall() {
    std::string error;
    if (install_.Finish(&error) == trg::Task::State::kIdle) {
      return;
    }
    const std::shared_ptr<InstallOutcome> outcome = std::move(outcome_);
    if (!outcome) {
      return;
    }
    const InstallResult& result = outcome->result;
    const InstallMessage message = DescribeInstall(result.status);
    if (result.ok() || result.status == InstallStatus::kCancelled) {
      if (result.ok()) {
        game_files_ = outcome->files.state;
        REXLOG_INFO("Game files installed");
      } else {
        REXLOG_INFO("Game install cancelled");
      }
      launcher_->SetStatus(message.title + ". " + message.text, 6.0);
      return;
    }
    REXLOG_WARN("Game install failed: {} ({})", message.title, result.detail);
    install_failure_ = message.title;
    install_failure_text_ = message.text;
    launcher_->SetStatus("Install failed: " + install_failure_, 8.0);
  }

  void PageDisplay(trg::Ui& ui) {
    ui.Toggle("Display mode",
              "How the game starts. Borderless Fullscreen fills the monitor. F11 switches while playing.", "fullscreen",
              false, "Windowed", "Borderless Fullscreen");
    ui.Row("Window size", "The game window's size in Windowed mode.");
    WindowSizeCombo();
    ui.Row("Monitor", "The display the game opens on.");
    MonitorCombo();
    if (ui.Combo("Aspect Ratio", "Sets the game's target rendering format and the Window size choices.",
                 "crackdown_aspect_ratio", "16:9", AspectRatioOptions())) {
      SelectWindowSizeForAspect();
    }
    ui.Row("VSync", "On prevents tearing. Off allows it, for the lowest latency and G-Sync/FreeSync.");
    {
      const bool tearing = settings_.GetBool(kTearingSetting, true);
      if (int i = ui.Segmented("vsync", {"Off", "On"}, tearing ? 0 : 1); i >= 0) {
        settings_.SetBool(kTearingSetting, i == 0);
      }
    }
    ui.Row(kPresentationLabel, kPresentationHelp);
    {
      const bool letterbox = settings_.GetBool(kLetterboxSetting, true);
      if (int i = ui.Segmented("presentation", {"Letterbox", "Stretch"}, LetterboxSegment(letterbox)); i >= 0) {
        settings_.SetBool(kLetterboxSetting, LetterboxFromSegment(i));
      }
    }
  }

  const MonitorInfo* SelectedMonitor() const {
    const int index = settings_.GetInt("monitor", 0);
    if (index >= 1 && index <= int(monitors_.size())) {
      return &monitors_[size_t(index - 1)];
    }
    for (const MonitorInfo& m : monitors_) {
      if (m.primary) {
        return &m;
      }
    }
    return monitors_.empty() ? nullptr : &monitors_.front();
  }

  AspectRatio SelectedAspectRatio() const {
    return ParseAspectRatio(settings_.Get("crackdown_aspect_ratio", "16:9"));
  }

  // Whether a Window size fits the display at its scale, so the game window is not larger than the screen.
  static bool WindowSizeFits(int width, int height, const MonitorInfo* monitor) {
    return !monitor || (width * monitor->scale < monitor->width && height * monitor->scale < monitor->height);
  }

  void SelectWindowSizeForAspect() {
    const MonitorInfo* monitor = SelectedMonitor();
    const WindowSize current{settings_.GetInt("window_width", 0), settings_.GetInt("window_height", 0)};
    const WindowSize chosen = WindowSizeForAspect(SelectedAspectRatio(), current, [monitor](WindowSize size) {
      return WindowSizeFits(size.width, size.height, monitor);
    });
    if (chosen.width != current.width || chosen.height != current.height) {
      settings_.SetInt("window_width", chosen.width);
      settings_.SetInt("window_height", chosen.height);
    }
  }

  // window_width/height are logical pixels; display scaling enlarges them.
  void WindowSizeCombo() {
    const int width = settings_.GetInt("window_width", 0);
    const int height = settings_.GetInt("window_height", 0);
    const AspectRatio aspect = SelectedAspectRatio();
    const MonitorInfo* monitor = SelectedMonitor();
    const float scale = monitor ? monitor->scale : 1.0f;
    auto label = [scale](int w, int h) {
      if (w <= 0 || h <= 0) {
        return std::string("Default");
      }
      std::string text = Size(w, h);
      if (scale != 1.0f) {
        text += "   (" + Size(int(std::lround(w * scale)), int(std::lround(h * scale))) + " on screen)";
      }
      return text;
    };
    if (ImGui::BeginCombo("##window_size", label(width, height).c_str())) {
      for (const AspectWindowSize& size : kWindowSizes) {
        if (size.aspect != aspect) {
          continue;
        }
        const bool current = size.width == width && size.height == height;
        if (!current && !WindowSizeFits(size.width, size.height, monitor)) {
          continue;
        }
        if (ImGui::Selectable(label(size.width, size.height).c_str(), current) && !current) {
          settings_.SetInt("window_width", size.width);
          settings_.SetInt("window_height", size.height);
        }
      }
      ImGui::EndCombo();
    }
  }

  void MonitorCombo() {
    const int current = settings_.GetInt("monitor", 0);
    auto label = [this](int index) {
      if (index <= 0) {
        return std::string("Default");
      }
      if (index > int(monitors_.size())) {
        return "Display " + std::to_string(index) + " (not connected)";
      }
      const MonitorInfo& m = monitors_[size_t(index - 1)];
      return "Display " + std::to_string(index) + (m.primary ? " (primary)" : "") + "   " + Size(m.width, m.height);
    };
    if (ImGui::BeginCombo("##monitor", label(current).c_str())) {
      for (int i = 1; i <= int(monitors_.size()); ++i) {
        if (ImGui::Selectable(label(i).c_str(), i == current) && i != current) {
          settings_.SetInt("monitor", i);
        }
      }
      ImGui::EndCombo();
    }
  }

  void PageGraphics(trg::Ui& ui) {
    if (!graphics_adapters_.empty()) {
      std::vector<trg::Option> adapters;
      for (const auto& [value, label] : GraphicsAdapterOptions(
               graphics_adapters_, settings_.Get("crackdown_graphics_adapter", kGraphicsAdapterAuto))) {
        adapters.push_back({value, label});
      }
      ui.Combo("Graphics card",
               "Selects which GPU renders the game. Automatic uses the recommended high-performance GPU. "
               "A GPU chosen here overrides automatic selection and restarts the game window.",
               "crackdown_graphics_adapter", kGraphicsAdapterAuto, adapters);
    }
    ui.Combo("Render resolution",
             "Draws the game at a multiple of its own picture, then scales it to the window. Higher is sharper and "
             "costs more GPU time. Window size is separate.",
             "resolution_scale", "1", RenderResolutionOptions(SelectedAspectRatio()));
    ui.Combo("Internal render resolution",
             "Renders the game internally at a lower resolution, then scales it to the selected render resolution. "
             "Lower values improve GPU performance but reduce image sharpness.",
             "internal_render_scale", "100",
             {{"100", "100% Native"}, {"75", "75% Performance"}, {"50", "50% Low-end"}});
    ui.Choice("Anti-aliasing", "Smooths jagged edges after the frame is drawn. Extreme is softer but cleaner.",
              "swap_post_effect", "none", {{"none", "Off"}, {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA Extreme"}});
    ui.Toggle("Multisampling",
              "The game draws parts of the scene with multisampling, which cannot be turned off. 2\xC3\x97 MSAA is "
              "fastest; try 4\xC3\x97 only if the 2\xC3\x97 parts show graphics problems.",
              "crackdown_msaa_compatibility", false, "2\xC3\x97 MSAA", "4\xC3\x97 MSAA (Compatibility)");
    ui.Choice("Texture filtering",
              "Anisotropic filtering keeps textures sharp at oblique angles. Game leaves it to the game.",
              "anisotropic_override", "3",
              {{"-1", "Game"},
               {"0", "Off"},
               {"2", "2\xC3\x97"},
               {"3", "4\xC3\x97"},
               {"4", "8\xC3\x97"},
               {"5", "16\xC3\x97"}},
              6);
  }

  static void PageGameplay(trg::Ui& ui) {
    ui.Toggle("Skip intro", "Start directly at the title screen", "crackdown_skip_intro", false, "Off", "On");
    std::vector<trg::Option> languages;
    for (const GameLanguage& language : kGameLanguages) {
      languages.push_back({language.key, language.label});
    }
    ui.Combo(kLanguageLabel, kLanguageHelp, kLanguageSetting, kDefaultLanguage, languages);
    std::vector<trg::Option> frame_rates;
    for (const auto& [value, label] : FrameRateOptions()) {
      frame_rates.push_back({value, label});
    }
    ui.Combo("Frame rate",
             "The most frames per second the game draws. Game speed stays the same at every rate; 30 is the "
             "console's own timing.",
             "crackdown_frame_rate", "60", frame_rates);
    ui.Toggle("Frame rate in title", "Shows the game's frame rate in the window title: Crackdown Recompiled | FPS: 60.00",
              "crackdown_show_fps", true, "Off", "On");
    ui.Toggle("Render resolution in title", "Shows the game's render resolution in the window title",
              "crackdown_show_render_resolution", false, "Off", "On");
    ui.Toggle("Frame counter", "Shows the game's frame rate in the corner. F2 toggles it while playing.",
              "crackdown_show_frame_counter", false, "Hidden", "Shown");
  }

  static std::vector<trg::Option> CheatOptions(cheats::Kind kind, std::span<const int> values) {
    std::vector<trg::Option> options;
    for (const int value : values) {
      options.push_back({std::to_string(value), cheats::ValueLabel(kind, value)});
    }
    return options;
  }

  static std::vector<trg::Option> MultiplierOptions(std::span<const int> values) {
    return CheatOptions(cheats::Kind::Multiplier, values);
  }

  static void PageCheats(trg::Ui& ui) {
    ui.Paragraph("Cheats start with these values. F10 in the game changes them for that session only.");
    ui.Heading("PLAYER");
    ui.Toggle("Infinite Health", "Your character takes no damage.", "cheat_infinite_health", false, "Off", "On");
    ui.Combo("Movement Speed", "How much faster your character moves on foot.", "cheat_movement_speed", "0",
             MultiplierOptions(cheats::kMovementValues));
    ui.Combo("Jump Height", "How much higher your character jumps.", "cheat_jump_height", "0",
             MultiplierOptions(cheats::kMovementValues));
    ui.Toggle("No Fall Damage", "Falling does not hurt your character.", "cheat_no_fall_damage", false, "Off", "On");
    ui.Heading("WEAPONS");
    ui.Toggle("Infinite Ammo", "Weapons never run out of ammunition.", "cheat_infinite_ammo", false, "Off", "On");
    ui.Heading("VEHICLE");
    ui.Toggle("Infinite Vehicle Health", "The vehicle you drive takes no damage.", "cheat_infinite_vehicle_health",
              false, "Off", "On");
    ui.Combo("Vehicle Speed", "How much faster the vehicle you drive goes.", "cheat_vehicle_speed", "0",
             MultiplierOptions(cheats::kVehicleValues));
    ui.Heading("MAP");
    ui.Combo("Agility Orbs on Map", "Marks the agility orbs you have not collected yet on the minimap, and on the "
             "pause menu's map too if you choose.", "cheat_agility_orbs_on_map", "0",
             CheatOptions(cheats::Kind::MapDisplay, cheats::kMapValues));
    ui.Combo("Hidden Orbs on Map", "Marks the hidden orbs you have not collected yet on the minimap, and on the "
             "pause menu's map too if you choose.", "cheat_hidden_orbs_on_map", "0",
             CheatOptions(cheats::Kind::MapDisplay, cheats::kMapValues));
  }

  static void PageControls(trg::Ui& ui) {
    ui.Info("Controller", "Automatic: Xbox controllers work without setup.");
    ui.Toggle("Keyboard and mouse", "Use keyboard and mouse for gameplay controls together with the controller.",
              "mnk_mode", false, "Off", "On");
    ImGui::BeginDisabled(!ui.settings().GetBool("mnk_mode", false));
    ui.SliderFloat("Mouse sensitivity", "How fast the mouse turns the camera.", "mnk_sensitivity",
                   float(mouse_look::kDefaultSensitivity), float(mouse_look::kMinSensitivity),
                   float(mouse_look::kMaxSensitivity), "%.2fx");
    ImGui::EndDisabled();
    if (ui.ConfirmRow("Keyboard and mouse defaults",
                      "Restores the default keyboard controls and mouse sensitivity.", "Reset to defaults",
                      "Put the keyboard controls and mouse sensitivity back to their defaults?", "Reset")) {
      ResetKeyboardAndMouseDefaults(ui.settings());
      ui.SetStatus("Keyboard and mouse controls reset to defaults.");
    }
    if (ui.Section("Keyboard controls", true)) {
      ui.Help("Click a key, then press the new one. Esc cancels.");
      const bool enabled = ui.settings().GetBool("mnk_mode", false);
      if (!enabled) {
        ImGui::BeginDisabled();
      }
      for (const KeyboardAction& action : kKeyboardActions) {
        KeyboardRow(ui, action);
      }
      if (!enabled) {
        ImGui::EndDisabled();
      }
    }
  }

  static void KeyboardRow(trg::Ui& ui, const KeyboardAction& action) {
    const std::string shown = KeyboardBindingLabel(SavedKeyboardBinding(ui.settings(), action));
    const std::optional<ImGuiKey> key = ui.KeyBindRow(action.label, nullptr, shown);
    if (!key) {
      return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const std::string binding = KeyboardBindingFor(*key, io.KeyShift, io.KeyCtrl, io.KeyAlt);
    if (binding.empty()) {
      ui.SetStatus("That key cannot be used.");
      return;
    }
    ui.settings().Set(action.key, binding);
  }

  void PageAbout(trg::Ui& ui) {
    ui.Paragraph(
        "Crackdown for Xbox 360, statically recompiled to run natively on PC with the ReXGlue SDK. No game code or "
        "assets are included: the game runs from your own game files.");
    ui.Spacer(4);
    if (!paths_.game_dir.empty()) {
      ui.FolderRow("Game files", "Where the game data is read from.", "Open game folder", Utf8(paths_.game_dir));
    }
    ui.FolderRow("Saves and settings",
                 paths_.user_data_outcome == UserDataOutcome::kExplicit
                     ? "A folder given with --user_data_root."
                     : "Saves, settings and caches live next to the program. Copy this folder to move them.",
                 "Open data folder", Utf8(paths_.user_dir));
    if (ui.ButtonRow("Settings file", "Your launcher choices, as plain text. Loaded after crackdown.toml.",
                     "Show settings file") &&
        launcher_->Save()) {
      std::error_code ec;
      const bool exists = std::filesystem::exists(paths_.settings_file, ec);
      trg::OpenInFileManager(Utf8(exists ? paths_.settings_file : paths_.user_dir));
    }
    ui.ResetAllRow();
    ui.Heading("Diagnostics");
    if (ui.Toggle("Detailed logging", "Keeps logs for normal sessions. Crash diagnostics are always preserved.",
                  "crackdown_detailed_logging", true)) {
      rex::cvar::SetFlagByName("crackdown_detailed_logging", settings_.Get("crackdown_detailed_logging", "true"));
      ApplyDetailedLogging();
    }
    if (ui.ButtonRow("Logs folder", "Open the folder containing session and crash logs", "Open logs folder")) {
      OpenLogsFolder();
    }
    ui.Info("Port version", CRACKDOWN_PORT_VERSION);
    ui.Info("Libraries", "ReXGlue SDK " REXGLUE_VERSION_STRING "   \xC2\xB7   TRG Launcher " CRACKDOWN_TRG_VERSION);
    ui.Info("Game", "Xbox 360 disc, title 4D5307DC, game version TU0");
    ui.Link(DisplayUrl(kRepositoryUrl).c_str(), kRepositoryUrl);
  }

  void OpenLogsFolder() {
    std::error_code ec;
    std::filesystem::path folder = paths_.logs_dir;
    std::filesystem::create_directories(folder, ec);
    if (!std::filesystem::is_directory(folder, ec)) {
      REXLOG_WARN("Could not create the logs folder {}: {}", Utf8(folder), ec.message());
      folder = folder.parent_path();
    }
    trg::OpenInFileManager(Utf8(folder));
  }

  LauncherPaths paths_;
  LauncherCallbacks cb_;
  GameSettings& settings_;
  LauncherGameContext game_;
  GameFiles::State game_files_;
  std::vector<AchievementEntry> entries_;
  std::unique_ptr<rex::ui::AchievementIconCache> icons_;
  std::vector<MonitorInfo> monitors_;
  std::vector<GraphicsAdapter> graphics_adapters_;
  std::unique_ptr<rex::ui::ImmediateTexture> header_art_;
  std::unique_ptr<trg::Launcher> launcher_;
  LauncherGamepad gamepad_;
  bool nav_taken_ = false;
  bool starting_ = false;
  double play_started_ = 0.0;
  ImGuiConfigFlags saved_nav_flags_ = 0;
  ImGuiBackendFlags saved_has_gamepad_ = 0;
  bool done_ = false;
  std::optional<WindowPlacement> startup_;
  PlacementTracker placement_;
  FullscreenGate fullscreen_gate_;
  LauncherFullscreenPreference fullscreen_pref_{SavedLauncherFullscreen()};
  bool restore_when_windowed_ = false;
  bool leave_requested_ = false;
  bool leave_gave_up_ = false;
  double leave_started_ = 0.0;
  std::string install_failure_;
  std::string install_failure_text_;
  std::shared_ptr<InstallOutcome> outcome_;
  trg::Task install_;
  std::unique_ptr<trg::update::UpdateController> updater_;
  bool skip_finish_save_ = false;
};

bool g_skipped_once = false;

std::pair<int, int> LauncherWindowSize() {
  const MonitorInfo primary = MonitorForSetting(1);
  if (primary.width == 0) {
    return kLauncherWindowSize;
  }
  const DpiWindowSize size = FitWindowToWorkArea(kLauncherWindowSize.first, kLauncherWindowSize.second,
                                                 primary.work_width, primary.work_height, primary.scale,
                                                 kLauncherMaxShare);
  return {size.width, size.height};
}

}  // namespace

void LoadLauncherFonts(ImFontAtlas* atlas, float density) {
  // Latin-1 plus General Punctuation.
  static const ImWchar kRanges[] = {0x0020, 0x00FF, 0x2010, 0x205E, 0};
  density = ValidDpiScale(density);
  g_font_density = density;
  ImFontConfig config;
  config.RasterizerDensity = density;
  auto load = [&](const std::filesystem::path& path) -> ImFont* {
    std::error_code ec;
    if (path.empty() || !std::filesystem::exists(path, ec)) {
      return nullptr;
    }
    return atlas->AddFontFromFileTTF(Utf8(path).c_str(), g_fonts.size, &config, kRanges);
  };
  const UiFontFiles fonts = FindUiFonts();
  g_fonts.regular = load(fonts.regular);
  g_fonts.semibold = load(fonts.semibold);
  g_fonts.bold = load(fonts.bold);
}

void UpdateLauncherFonts(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate_drawer, float density) {
  if (!drawer || !immediate_drawer || !g_fonts.regular) {
    return;
  }
  density = ValidDpiScale(density);
  if (std::abs(density - g_font_density) < 0.001f) {
    return;
  }

  ImGuiIO& io = drawer->GetIO();
  for (ImFont* font : {g_fonts.regular, g_fonts.semibold, g_fonts.bold}) {
    if (!font) {
      continue;
    }
    const ImWchar fallback = font->FallbackChar;
    const ImWchar ellipsis = font->EllipsisChar;
    font->ClearOutputData();
    font->FallbackChar = fallback;
    font->EllipsisChar = ellipsis;
    font->CurrentRasterizerDensity = density;
    for (ImFontConfig* source : font->Sources) {
      source->RasterizerDensity = density;
    }
  }
  io.Fonts->TexIsBuilt = false;
  // The drawer holds the uploaded atlas until the immediate drawer is replaced.
  drawer->SetImmediateDrawer(nullptr);
  drawer->SetImmediateDrawer(immediate_drawer);
  g_font_density = density;
}

bool ShouldShowLauncher(const std::filesystem::path& game_dir) {
  g_skipped_once = REXCVAR_GET(crackdown_skip_launcher);
  rex::cvar::ResetToDefault("crackdown_skip_launcher");
  return LauncherWanted(CheckAndLogGameFiles(game_dir).state == GameFiles::kReady, g_skipped_once, ShiftHeld(),
                        REXCVAR_GET(crackdown_launcher));
}

bool ShiftHeldAfterWindowOpened() {
#if defined(_WIN32)
  return false;
#else
  return !g_skipped_once && ShiftHeld();
#endif
}

void PrepareLauncherWindow() {
  const auto [width, height] = LauncherWindowSize();
  rex::cvar::SetFlagByName("fullscreen", "false");
  rex::cvar::SetFlagByName("window_width", std::to_string(width));
  rex::cvar::SetFlagByName("window_height", std::to_string(height));
  // Monitor 0 lets SDL centre the window on the primary display, whatever the display order.
  rex::cvar::SetFlagByName("monitor", "0");
  const WindowPlacement saved = SavedLauncherPlacement();
  InstallPreShowPlacement(PlanPreShow(saved, WorkAreas()), saved);
}

void ShowLauncher(rex::ui::ImGuiDrawer* drawer, LauncherPaths paths, GameSettings& settings,
                  LauncherGameContext game, LauncherCallbacks callbacks) {
  new LauncherDialog(drawer, std::move(paths), settings, game, std::move(callbacks));
}

void LauncherFileDropped(const std::filesystem::path& path) {
  if (g_dialog) {
    g_dialog->FileDropped(path);
  }
}

}  // namespace crackdown
