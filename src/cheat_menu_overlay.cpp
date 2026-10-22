#include "cheat_menu_overlay.h"

#include <algorithm>
#include <cfloat>
#include <utility>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "agency_accent.h"
#include "cheat_hooks.h"
#include "cheat_menu_input.h"
#include "cheats.h"
#include "frame_counter.h"
#include "settings.h"

namespace crackdown {
namespace {

ImVec4 Accent() { return ImVec4(kAccentRed, kAccentGreen, kAccentBlue, 1.0f); }

}  // namespace

CheatMenuOverlay::CheatMenuOverlay(rex::ui::ImGuiDrawer* imgui_drawer, Controls controls)
    : ImGuiDialog(imgui_drawer), controls_(std::move(controls)) {}

// The keys and buttons that went down while the menu was open, until they come up again.
void CheatMenuOverlay::TrackKeys() {
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
    if (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY) {
      continue;
    }
    const size_t index = size_t(key - ImGuiKey_NamedKey_BEGIN);
    if (ImGui::IsKeyPressed(ImGuiKey(key), false)) {
      pressed_.set(index);
    } else if (!ImGui::IsKeyDown(ImGuiKey(key))) {
      pressed_.reset(index);
    }
  }
}

void CheatMenuOverlay::ActivateSelected() {
  if (!cheats::Available()) {
    return;
  }
  const cheats::Cheat cheat = cheats::kCheats[size_t(selected_)].cheat;
  const cheats::CheatInfo& info = cheats::InfoOf(cheat);
  const int value = cheats::Cycle(cheat);
  REXLOG_INFO("Cheats: {} {} (this session)", info.label, cheats::ValueLabel(info.kind, value));
}

void CheatMenuOverlay::ReadInput() {
  int step = 0;
  bool activate = false;
  bool close = false;
  while (const uint8_t event = cheats::TakeMenuNavEvent()) {
    step += event == cheats::kNavUp ? -1 : event == cheats::kNavDown ? 1 : 0;
    activate = activate || event == cheats::kNavActivate;
    close = close || event == cheats::kNavClose;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
    --step;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
    ++step;
  }
  activate = activate || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
  close = close || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  selected_ = cheats::MoveSelection(selected_, step);
  if (close) {
    close_.Request();
  } else if (activate) {
    ActivateSelected();
  }
}

void CheatMenuOverlay::DrawEntry(const cheats::MenuEntry& entry, int row, float scale, const ImVec4& accent,
                                 const ImVec4& focus, const ImVec4& dim) {
  if (entry.heading) {
    ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
    ImGui::TextColored(dim, "%s", entry.label);
    return;
  }
  const cheats::CheatInfo& info = cheats::InfoOf(entry.cheat);
  const float height = ImGui::GetFrameHeight() + 4.0f * scale;
  ImGui::PushID(row);
  ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, height));
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  if (ImGui::IsItemClicked()) {
    selected_ = row;
    ActivateSelected();
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (selected_ == row) {
    draw->AddRectFilled(min, max, ImGui::GetColorU32(focus), 3.0f * scale);
    // On a window too short for every row, the panel scrolls to keep the selection in view.
    if (!ImGui::IsRectVisible(min, max)) {
      ImGui::SetScrollHereY();
    }
  }
  const float pad = ImGui::GetFontSize() * 0.4f;
  const float y = min.y + (height - ImGui::GetFontSize()) * 0.5f;
  const bool available = cheats::Available();
  const int value = cheats::Value(entry.cheat);
  const char* state = available ? cheats::ValueLabel(info.kind, value) : cheats::kMenuUnavailable;
  const bool lit = available && value != 0;
  draw->AddText(ImVec2(min.x + pad, y), ImGui::GetColorU32(ImGuiCol_Text), entry.label);
  draw->AddText(ImVec2(max.x - pad - ImGui::CalcTextSize(state).x, y), ImGui::GetColorU32(lit ? accent : dim), state);
  ImGui::PopID();
}

void CheatMenuOverlay::DrawPanel(ImGuiIO& io) {
  const ImVec4 accent = Accent();
  const ImVec4 dim(0.64f, 0.66f, 0.76f, 1.0f);
  const ImVec4 focus(accent.x, accent.y, accent.z, 0.42f);
  const float base_size = ImGui::GetStyle().FontSizeBase;
  const float scale = cheats::MenuScale(1.0f, io.DisplaySize.x, base_size);
  const float em = base_size * scale;
  const float margin = em * 0.8f;
  float top = margin;
  if (FrameCounterVisible(REXCVAR_GET(crackdown_show_frame_counter), GetGuestFrameStats())) {
    top += ImGui::GetFontSize() * 2.6f;
  }
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, top), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(em * cheats::kMenuWidthEm, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, std::max(io.DisplaySize.y - top - margin, em)));
  if (!focused_) {
    focused_ = true;
    ImGui::SetNextWindowFocus();
  }
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.11f, 0.92f));
  ImGui::PushStyleColor(ImGuiCol_Border, accent);
  const ImGuiStyle& style = ImGui::GetStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 0.7f, em * 0.5f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x * scale, style.FramePadding.y * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * scale, style.ItemSpacing.y * scale));
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("##crackdown_cheat_menu", nullptr, kFlags)) {
    ImGui::PushFont(nullptr, em);
    ImGui::TextColored(accent, "%s", cheats::kMenuTitle);
    ImGui::Separator();
    int row = 0;
    for (const cheats::MenuEntry& entry : cheats::kMenuEntries) {
      DrawEntry(entry, entry.heading ? -1 : row, scale, accent, focus, dim);
      row += entry.heading ? 0 : 1;
    }
    ImGui::Separator();
    ImGui::TextColored(dim, "%s", cheats::kMenuCloseHint);
    ImGui::TextColored(dim, "%s", cheats::kMenuNote);
    ImGui::PopFont();
  }
  ImGui::End();
  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(2);
}

void CheatMenuOverlay::OnDraw(ImGuiIO& io) {
  TrackKeys();
  if (!close_.requested()) {
    ReadInput();
  }
  if (close_.requested()) {
    if (close_.Step(cheats::MenuInputHeld() || pressed_.any(), ImGui::GetTime())) {
      controls_.finished();
    }
    return;
  }
  DrawPanel(io);
}

}  // namespace crackdown
