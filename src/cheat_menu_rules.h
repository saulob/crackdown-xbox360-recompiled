#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "achievements_overlay_rules.h"
#include "cheats.h"

namespace crackdown::cheats {

inline constexpr char kMenuTitle[] = "CHEATS";
inline constexpr char kMenuCloseHint[] = "F10 Close";
inline constexpr char kMenuNote[] = "Not saved to the launcher";
inline constexpr char kMenuUnavailable[] = "Unavailable";

// A heading names a section; every other entry is a cheat row the selection can reach.
struct MenuEntry {
  bool heading;
  Cheat cheat;
  const char* label;
};

// One heading per run of cheats in the same section.
constexpr size_t CountSections() {
  size_t sections = 0;
  for (size_t i = 0; i < kCheatCount; ++i) {
    if (i == 0 || kCheats[i].section != kCheats[i - 1].section) {
      ++sections;
    }
  }
  return sections;
}

inline constexpr size_t kMenuEntryCount = kCheatCount + CountSections();

constexpr std::array<MenuEntry, kMenuEntryCount> MakeMenuEntries() {
  std::array<MenuEntry, kMenuEntryCount> entries{};
  size_t next = 0;
  bool have_section = false;
  Section current{};
  for (const CheatInfo& info : kCheats) {
    if (!have_section || info.section != current) {
      entries[next++] = {true, Cheat{}, SectionLabel(info.section)};
      current = info.section;
      have_section = true;
    }
    entries[next++] = {false, info.cheat, info.label};
  }
  return entries;
}

inline constexpr std::array<MenuEntry, kMenuEntryCount> kMenuEntries = MakeMenuEntries();

// Selection moves between cheat rows and stops at the first and last.
inline constexpr int MoveSelection(int selected, int step) {
  return std::clamp(selected + step, 0, static_cast<int>(kCheatCount) - 1);
}

// The panel is kMenuWidthEm wide. It grows with the display's content scale, a step below it (kMenuScaleStep) and never
// under kMenuMinScale, and never takes more than kMenuMaxWidthShare of the screen.
inline constexpr float kMenuWidthEm = 24.0f;
inline constexpr float kMenuMaxScale = 3.0f;
inline constexpr float kMenuScaleStep = 0.75f;
inline constexpr float kMenuMinScale = 1.25f;
inline constexpr float kMenuMaxWidthShare = 0.45f;

constexpr float MenuScale(float display_scale, float screen_width, float em_pixels) {
  const float display = display_scale < 1.0f ? 1.0f : (display_scale > kMenuMaxScale ? kMenuMaxScale : display_scale);
  const float stepped = display * kMenuScaleStep;
  const float wanted = stepped < kMenuMinScale ? kMenuMinScale : stepped;
  const float fits = kMenuMaxWidthShare * screen_width / (kMenuWidthEm * em_pixels);
  return wanted < fits ? wanted : (fits > 1.0f ? fits : 1.0f);
}

// A button that is never reported released (a controller unplugged as the menu closes) cannot block the game forever.
inline constexpr double kReleaseTimeoutSeconds = 1.0;

inline bool InputReleased(bool anything_held, double waited_seconds, double timeout_seconds) {
  return !anything_held || waited_seconds >= timeout_seconds;
}

// Closing the menu (B, Escape, F10) starts a wait, and the game gets its input back only after what was pressed in
// the menu is released, so the closing press never also reaches the game.
class CloseGate {
 public:
  void Request() { requested_ = true; }
  bool requested() const { return requested_; }

  // Once per frame. True when the menu may go and the game may read its input again.
  bool Step(bool anything_held, double now) {
    if (!requested_) {
      return false;
    }
    if (since_ < 0.0) {
      since_ = now;
    }
    return InputReleased(anything_held, now - since_, kReleaseTimeoutSeconds);
  }

 private:
  bool requested_ = false;
  double since_ = -1.0;
};

inline constexpr uint16_t kXinputButtonA = 0x1000;

enum NavEvent : uint8_t {
  kNavUp = 1,
  kNavDown = 2,
  kNavActivate = 4,
  kNavClose = 8,
};

inline constexpr double kNavRepeatDelaySeconds = 0.4;
inline constexpr double kNavRepeatIntervalSeconds = 0.12;

// What the menu does with each controller read while it is up. D-pad or left stick up and down move, A activates
// and B closes. Every read of every pad is one Poll; the whole pad reads neutral to the game while the menu is up.
class CheatPadGate {
 public:
  static constexpr uint32_t kUsers = 4;

  struct Verdict {
    bool hide_input = false;
    uint8_t events = 0;
  };

  void SetOpen(bool open) {
    open_ = open;
    direction_ = {};
  }

  bool open() const { return open_; }

  bool AnyHeld() const {
    return std::any_of(held_.begin(), held_.end(), [](bool held) { return held; });
  }

  Verdict Poll(uint32_t user, uint16_t buttons, int thumb_x, int thumb_y, double now) {
    Verdict verdict;
    verdict.hide_input = open_;
    if (user >= kUsers) {
      return verdict;
    }
    held_[user] = buttons != 0 || std::abs(thumb_x) > kXinputThumbDeadzone || std::abs(thumb_y) > kXinputThumbDeadzone;
    const uint16_t pressed = static_cast<uint16_t>(buttons & ~previous_[user]);
    previous_[user] = buttons;
    if (!open_) {
      direction_[user] = 0;
      return verdict;
    }
    if (pressed & kXinputButtonA) {
      verdict.events |= kNavActivate;
    }
    if (pressed & kXinputButtonB) {
      verdict.events |= kNavClose;
    }
    verdict.events |= Repeat(user, DirectionOf(buttons, thumb_y), now);
    return verdict;
  }

 private:
  static int DirectionOf(uint16_t buttons, int thumb_y) {
    if (buttons & kXinputDpadUp) return -1;
    if (buttons & kXinputDpadDown) return 1;
    if (thumb_y > kXinputThumbDeadzone) return -1;
    if (thumb_y < -kXinputThumbDeadzone) return 1;
    return 0;
  }

  uint8_t Repeat(uint32_t user, int direction, double now) {
    if (direction == 0) {
      direction_[user] = 0;
      return 0;
    }
    const uint8_t event = direction < 0 ? kNavUp : kNavDown;
    if (direction != direction_[user]) {
      direction_[user] = direction;
      next_repeat_[user] = now + kNavRepeatDelaySeconds;
      return event;
    }
    if (now >= next_repeat_[user]) {
      next_repeat_[user] = now + kNavRepeatIntervalSeconds;
      return event;
    }
    return 0;
  }

  bool open_ = false;
  std::array<bool, kUsers> held_{};
  std::array<uint16_t, kUsers> previous_{};
  std::array<int, kUsers> direction_{};
  std::array<double, kUsers> next_repeat_{};
};

}  // namespace crackdown::cheats
