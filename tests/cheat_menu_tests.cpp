#include <cmath>
#include <cstring>

#include "cheat_menu_rules.h"
#include "check.h"

using namespace crackdown;
using namespace crackdown::cheats;

namespace {

void TestMenuEntriesFollowTheSpecifiedLayout() {
  const char* expected[] = {"PLAYER",
                            "Infinite Health",
                            "Movement Speed",
                            "Jump Height",
                            "No Fall Damage",
                            "WEAPONS",
                            "Infinite Ammo",
                            "VEHICLE",
                            "Infinite Vehicle Health",
                            "Vehicle Speed",
                            "MAP",
                            "Agility Orbs on Map",
                            "Hidden Orbs on Map"};
  CHECK(kMenuEntries.size() == std::size(expected));
  for (size_t i = 0; i < kMenuEntries.size(); ++i) {
    CHECK(std::strcmp(kMenuEntries[i].label, expected[i]) == 0);
  }
  CHECK(kMenuEntries[0].heading && !kMenuEntries[1].heading);
  CHECK(std::strcmp(kMenuTitle, "CHEATS") == 0);
  CHECK(std::strcmp(kMenuCloseHint, "F10 Close") == 0);
  CHECK(std::strcmp(kMenuNote, "Not saved to the launcher") == 0);
}

void TestCheatRowsKeepTheOrderOfTheCheatTable() {
  size_t row = 0;
  for (const MenuEntry& entry : kMenuEntries) {
    if (entry.heading) {
      continue;
    }
    CHECK(entry.cheat == kCheats[row].cheat);
    ++row;
  }
  CHECK(row == kCheatCount);
}

void TestMovementAndJumpCycleOffTo10xAndBack() {
  for (const Cheat cheat : {Cheat::MovementSpeed, Cheat::JumpHeight}) {
    ResetSession();
    SetSavedValue(cheat, 0);
    const int expected[] = {2, 4, 6, 8, 10, 0};
    const int factors[] = {2, 4, 6, 8, 10, 1};
    for (int i = 0; i < 6; ++i) {
      CHECK(Cycle(cheat) == expected[i]);
      CHECK(Factor(cheat) == factors[i]);
    }
    CHECK(!Enabled(cheat));
  }
  CHECK(std::strcmp(ValueLabel(Kind::Multiplier, 8), "8x") == 0);
  CHECK(std::strcmp(ValueLabel(Kind::Multiplier, 10), "10x") == 0);
  ResetSession();
}

void TestVehicleSpeedStopsAt6x() {
  ResetSession();
  SetSavedValue(Cheat::VehicleSpeed, 0);
  CHECK(Cycle(Cheat::VehicleSpeed) == 2);
  CHECK(Cycle(Cheat::VehicleSpeed) == 4);
  CHECK(Cycle(Cheat::VehicleSpeed) == 6);
  CHECK(Cycle(Cheat::VehicleSpeed) == 0);
  CHECK(Cycle(Cheat::VehicleSpeed) == 2);
  ResetSession();
  SetSavedValue(Cheat::VehicleSpeed, 8);
  CHECK(Value(Cheat::VehicleSpeed) == 0);
  CHECK(Cycle(Cheat::VehicleSpeed) == 2);
  ResetSession();
}

void TestToggleCyclesBetweenOffAndOn() {
  ResetSession();
  SetSavedValue(Cheat::InfiniteHealth, 0);
  CHECK(Cycle(Cheat::InfiniteHealth) == 1);
  CHECK(Enabled(Cheat::InfiniteHealth));
  CHECK(Cycle(Cheat::InfiniteHealth) == 0);
  CHECK(std::strcmp(ValueLabel(Kind::Toggle, 1), "On") == 0);
  CHECK(std::strcmp(ValueLabel(Kind::Toggle, 0), "Off") == 0);
  ResetSession();
}

void TestMapDisplayCyclesOffMinimapWorldAndBack() {
  for (const Cheat cheat : {Cheat::AgilityOrbsOnMap, Cheat::HiddenOrbsOnMap}) {
    ResetSession();
    SetSavedValue(cheat, kMapOff);
    CHECK(Cycle(cheat) == kMapMinimap);
    CHECK(Cycle(cheat) == kMapMinimapAndWorld);
    CHECK(Cycle(cheat) == kMapOff);
    CHECK(!Enabled(cheat));
    SetSavedValue(cheat, 3);  // not offered: counts as Off
    CHECK(Value(cheat) == kMapOff);
  }
  CHECK(std::strcmp(ValueLabel(Kind::MapDisplay, kMapOff), "Off") == 0);
  CHECK(std::strcmp(ValueLabel(Kind::MapDisplay, kMapMinimap), "Minimap") == 0);
  CHECK(std::strcmp(ValueLabel(Kind::MapDisplay, kMapMinimapAndWorld), "Minimap + World") == 0);
  ResetSession();
}

void TestSavedOnBecomesTheStartingValue() {
  ResetSession();
  SetSavedValue(Cheat::InfiniteAmmo, 1);
  SetSavedValue(Cheat::MovementSpeed, 6);
  CHECK(Enabled(Cheat::InfiniteAmmo));
  CHECK(Factor(Cheat::MovementSpeed) == 6);
  ResetSession();
}

void TestSessionChangeDoesNotChangeTheSavedValue() {
  ResetSession();
  SetSavedValue(Cheat::JumpHeight, 4);
  CHECK(Cycle(Cheat::JumpHeight) == 6);
  CHECK(Value(Cheat::JumpHeight) == 6);
  ResetSession();
  CHECK(Value(Cheat::JumpHeight) == 4);
}

void TestResetSessionRestoresTheSavedValue() {
  ResetSession();
  SetSavedValue(Cheat::VehicleSpeed, 2);
  SetSavedValue(Cheat::InfiniteAmmo, 0);
  Cycle(Cheat::VehicleSpeed);
  Cycle(Cheat::InfiniteAmmo);
  ResetSession();
  CHECK(Value(Cheat::VehicleSpeed) == 2);
  CHECK(Value(Cheat::InfiniteAmmo) == 0);
  CHECK(!TakeRefillRequest(Cheat::InfiniteAmmo));
}

void TestTurningOnRequestsARefillOnce() {
  ResetSession();
  SetSavedValue(Cheat::InfiniteAmmo, 0);
  Cycle(Cheat::InfiniteAmmo);
  CHECK(TakeRefillRequest(Cheat::InfiniteAmmo));
  CHECK(!TakeRefillRequest(Cheat::InfiniteAmmo));
  ResetSession();
}

void TestMenuWidthFitsTheLongestLabel() {
  CHECK(kMenuWidthEm >= 22.0f && kMenuWidthEm <= 24.0f);
}

void TestSelectionStopsAtTheEnds() {
  CHECK(MoveSelection(0, -1) == 0);
  CHECK(MoveSelection(0, 1) == 1);
  CHECK(MoveSelection(int(kCheatCount) - 1, 1) == int(kCheatCount) - 1);
  CHECK(MoveSelection(3, 5) == int(kCheatCount) - 1);
}

void TestMenuScaleFollowsTheDisplayAndFitsTheScreen() {
  CHECK(std::fabs(MenuScale(1.0f, 1920.0f, 18.0f) - kMenuMinScale) < 1e-4f);
  CHECK(MenuScale(2.0f, 1920.0f, 18.0f) > kMenuMinScale);
  const float narrow = MenuScale(3.0f, 400.0f, 18.0f);
  CHECK(narrow * kMenuWidthEm * 18.0f <= 400.0f * kMenuMaxWidthShare + 1e-3f || narrow == 1.0f);
}

void TestCloseGateWaitsForReleaseAndTimesOut() {
  CloseGate gate;
  CHECK(!gate.Step(false, 1.0));
  gate.Request();
  CHECK(!gate.Step(true, 10.0));
  CHECK(gate.Step(false, 10.1));

  CloseGate stuck;
  stuck.Request();
  CHECK(!stuck.Step(true, 5.0));
  CHECK(!stuck.Step(true, 5.5));
  CHECK(stuck.Step(true, 5.0 + kReleaseTimeoutSeconds));
}

void TestPadGateHidesInputOnlyWhileOpen() {
  CheatPadGate gate;
  CHECK(!gate.Poll(0, kXinputButtonA, 0, 0, 0.0).hide_input);
  gate.SetOpen(true);
  CHECK(gate.Poll(0, 0, 0, 0, 0.1).hide_input);
  gate.SetOpen(false);
  CHECK(!gate.Poll(0, 0, 0, 0, 0.2).hide_input);
}

void TestPadGateEventsForButtons() {
  CheatPadGate gate;
  gate.SetOpen(true);
  gate.Poll(0, 0, 0, 0, 0.0);
  auto verdict = gate.Poll(0, kXinputButtonA, 0, 0, 0.1);
  CHECK(verdict.events == kNavActivate);
  CHECK(gate.Poll(0, kXinputButtonA, 0, 0, 0.2).events == 0);
  gate.Poll(0, 0, 0, 0, 0.3);
  CHECK(gate.Poll(0, kXinputButtonB, 0, 0, 0.4).events == kNavClose);
  CHECK(gate.Poll(0, kXinputButtonB, 0, 0, 0.5).events == 0);
}

void TestPadGateIgnoresButtonsHeldWhenTheMenuOpens() {
  CheatPadGate gate;
  gate.Poll(0, kXinputButtonA, 0, 0, 0.0);
  gate.SetOpen(true);
  CHECK(gate.Poll(0, kXinputButtonA, 0, 0, 0.1).events == 0);
  gate.Poll(0, 0, 0, 0, 0.2);
  CHECK(gate.Poll(0, kXinputButtonA, 0, 0, 0.3).events == kNavActivate);
}

void TestPadGateNavigatesWithDpadAndLeftStick() {
  CheatPadGate gate;
  gate.SetOpen(true);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, 0.0).events == kNavDown);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, 0.1).events == 0);
  CHECK(gate.Poll(0, 0, 0, 0, 0.2).events == 0);
  CHECK(gate.Poll(0, kXinputDpadUp, 0, 0, 0.3).events == kNavUp);
  CHECK(gate.Poll(0, 0, 0, 0, 0.4).events == 0);
  CHECK(gate.Poll(0, 0, 0, 20000, 0.5).events == kNavUp);
  CHECK(gate.Poll(0, 0, 0, 0, 0.6).events == 0);
  CHECK(gate.Poll(0, 0, 0, -20000, 0.7).events == kNavDown);
  CHECK(gate.Poll(0, 0, 0, kXinputThumbDeadzone - 1, 0.8).events == 0);
}

void TestPadGateRepeatsAHeldDirection() {
  CheatPadGate gate;
  gate.SetOpen(true);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, 0.0).events == kNavDown);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, kNavRepeatDelaySeconds - 0.01).events == 0);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, kNavRepeatDelaySeconds + 0.01).events == kNavDown);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, kNavRepeatDelaySeconds + 0.02).events == 0);
  CHECK(gate.Poll(0, kXinputDpadDown, 0, 0, kNavRepeatDelaySeconds + 0.01 + kNavRepeatIntervalSeconds + 0.001).events ==
        kNavDown);
}

void TestPadGateReportsAnythingHeld() {
  CheatPadGate gate;
  CHECK(!gate.AnyHeld());
  gate.Poll(1, kXinputButtonB, 0, 0, 0.0);
  CHECK(gate.AnyHeld());
  gate.Poll(1, 0, 0, 0, 0.1);
  CHECK(!gate.AnyHeld());
  gate.Poll(2, 0, 0, 30000, 0.2);
  CHECK(gate.AnyHeld());
  gate.Poll(2, 0, 100, 0, 0.3);
  CHECK(!gate.AnyHeld());
}

void TestPadGateIgnoresUnknownUsers() {
  CheatPadGate gate;
  gate.SetOpen(true);
  const auto verdict = gate.Poll(9, kXinputButtonA, 0, 0, 0.0);
  CHECK(verdict.hide_input && verdict.events == 0 && !gate.AnyHeld());
}

}  // namespace

int main() {
  TestMenuEntriesFollowTheSpecifiedLayout();
  TestCheatRowsKeepTheOrderOfTheCheatTable();
  TestMovementAndJumpCycleOffTo10xAndBack();
  TestVehicleSpeedStopsAt6x();
  TestToggleCyclesBetweenOffAndOn();
  TestMapDisplayCyclesOffMinimapWorldAndBack();
  TestSavedOnBecomesTheStartingValue();
  TestSessionChangeDoesNotChangeTheSavedValue();
  TestResetSessionRestoresTheSavedValue();
  TestTurningOnRequestsARefillOnce();
  TestMenuWidthFitsTheLongestLabel();
  TestSelectionStopsAtTheEnds();
  TestMenuScaleFollowsTheDisplayAndFitsTheScreen();
  TestCloseGateWaitsForReleaseAndTimesOut();
  TestPadGateHidesInputOnlyWhileOpen();
  TestPadGateEventsForButtons();
  TestPadGateIgnoresButtonsHeldWhenTheMenuOpens();
  TestPadGateNavigatesWithDpadAndLeftStick();
  TestPadGateRepeatsAHeldDirection();
  TestPadGateReportsAnythingHeld();
  TestPadGateIgnoresUnknownUsers();
  return check::Finish("cheat_menu_tests");
}
