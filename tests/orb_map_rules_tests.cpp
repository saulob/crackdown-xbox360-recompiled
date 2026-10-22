#include <cmath>
#include <limits>

#include "check.h"
#include "orb_map_rules.h"

using namespace crackdown::orb_map;

namespace {

bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

// The city view's numbers from the game: scale 0.1089, a slight turn, offset (461.5, 247).
ViewTransform CityView() {
  ViewTransform t;
  t.origin_x = 1900.0f;
  t.origin_z = -1600.0f;
  t.scale = 0.1089f;
  t.cos = std::cos(0.023f);
  t.sin = std::sin(0.023f);
  t.shift_x = 461.5f;
  t.shift_y = 247.0f;
  return t;
}

void TestWorldToViewMatchesTheGame() {
  // The view's origin lands on its shift; a point 1000 units east moves by scale * 1000 along the turned x axis.
  const ViewTransform t = CityView();
  const auto [ox, oy] = WorldToView(t, 1900.0f, -1600.0f);
  CHECK(Near(ox, 461.5f) && Near(oy, 247.0f));
  const auto [ex, ey] = WorldToView(t, 2900.0f, -1600.0f);
  CHECK(Near(ex, 461.5f + 108.9f * std::cos(0.023f)));
  CHECK(Near(ey, 247.0f + 108.9f * std::sin(0.023f)));
}

void TestViewToWorldInvertsWorldToView() {
  const ViewTransform t = CityView();
  for (const auto& [x, z] : {std::pair(286.0f, -3648.0f), std::pair(3778.0f, 340.0f), std::pair(1961.0f, -1866.0f)}) {
    const auto [mx, my] = WorldToView(t, x, z);
    const auto [wx, wz] = ViewToWorld(t, mx, my);
    CHECK(std::fabs(wx - x) < 0.1f && std::fabs(wz - z) < 0.1f);
  }
}

void TestPanelRejectsOutsideAndNaN() {
  CHECK(InsidePanel(kPanelLeft, kPanelTop));
  CHECK(InsidePanel((kPanelLeft + kPanelRight) * 0.5f, (kPanelTop + kPanelBottom) * 0.5f));
  CHECK(!InsidePanel(kPanelLeft - 1.0f, 300.0f));
  CHECK(!InsidePanel(600.0f, kPanelBottom + 1.0f));
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK(!InsidePanel(nan, 300.0f));
  CHECK(!InsidePanel(600.0f, nan));
}

void TestEachIslandBelongsToItsDistrict() {
  CHECK(DistrictAt(1045.0f, -1321.0f) == District::LosMuertos);
  CHECK(DistrictAt(1100.0f, -2711.0f) == District::LosMuertos);
  CHECK(DistrictAt(1961.0f, -1866.0f) == District::Agency);
  CHECK(DistrictAt(2318.0f, -2845.0f) == District::NorthEast);
  CHECK(DistrictAt(2427.0f, -428.0f) == District::SouthEast);
  CHECK(DistrictAt(3602.0f, -270.0f) == District::SouthEast);
}

void TestTheAgencyClaimsOnlyItsOwnIsland() {
  // Orbs on the north-east island's southern shore and on the bridge from Los Muertos are nearer the Agency's centre
  // than their own island's, but too far from it to be on the Agency's island.
  CHECK(DistrictAt(2435.0f, -2201.0f) == District::NorthEast);
  CHECK(DistrictAt(1920.0f, -2409.0f) == District::NorthEast);
  CHECK(DistrictAt(1390.0f, -1791.0f) == District::LosMuertos);
  CHECK(DistrictAt(1961.0f + 250.0f, -1866.0f) == District::Agency);
}

void TestLayoutFillsA16By9Window() {
  const LayoutToWindow m = MapLayoutToWindow(1920.0f, 1080.0f, 1920.0f, 1080.0f, true);
  CHECK(Near(m.left, 0.0f) && Near(m.top, 0.0f));
  CHECK(Near(m.scale_x, 1.5f) && Near(m.scale_y, 1.5f));
}

void TestLayoutStretchesAcrossAWideCanvas() {
  // 21:9 in a 2304 x 1080 window: a 1680 x 720 canvas letterboxed, the 1280-wide layout stretched across it.
  const LayoutToWindow m = MapLayoutToWindow(2304.0f, 1080.0f, 1680.0f, 720.0f, true);
  const float canvas_scale = 2304.0f / 1680.0f;
  CHECK(Near(m.left, 0.0f));
  CHECK(Near(m.top, (1080.0f - 720.0f * canvas_scale) * 0.5f));
  CHECK(Near(m.scale_x, canvas_scale * 1680.0f / 1280.0f));
  CHECK(Near(m.scale_y, canvas_scale));
}

void TestLayoutStretchFillsTheWindow() {
  const LayoutToWindow m = MapLayoutToWindow(1920.0f, 1080.0f, 1680.0f, 720.0f, false);
  CHECK(Near(m.left, 0.0f) && Near(m.top, 0.0f));
  CHECK(Near(m.scale_x, 1920.0f / 1280.0f) && Near(m.scale_y, 1.5f));
}

void TestLayoutNeedsAVideoMode() {
  const LayoutToWindow m = MapLayoutToWindow(1920.0f, 1080.0f, 0.0f, 0.0f, true);
  CHECK(m.scale_x == 0.0f && m.scale_y == 0.0f);
}

}  // namespace

int main() {
  TestWorldToViewMatchesTheGame();
  TestViewToWorldInvertsWorldToView();
  TestPanelRejectsOutsideAndNaN();
  TestEachIslandBelongsToItsDistrict();
  TestTheAgencyClaimsOnlyItsOwnIsland();
  TestLayoutFillsA16By9Window();
  TestLayoutStretchesAcrossAWideCanvas();
  TestLayoutStretchFillsTheWindow();
  TestLayoutNeedsAVideoMode();
  return check::Finish("orb_map_rules_tests");
}
