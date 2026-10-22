#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace crackdown::orb_map {

// --- Pause menu map ----------------------------------------------------------------------------------------------

// A map view's world-to-layout conversion (sub_826412E8): world (x, z) relative to `origin`, scaled, rotated by the
// view's angle, then shifted.
struct ViewTransform {
  float origin_x = 0.0f, origin_z = 0.0f;
  float scale = 0.0f;
  float cos = 1.0f, sin = 0.0f;
  float shift_x = 0.0f, shift_y = 0.0f;
};

inline std::pair<float, float> WorldToView(const ViewTransform& t, float x, float z) {
  const float px = (x - t.origin_x) * t.scale;
  const float py = (z - t.origin_z) * t.scale;
  return {px * t.cos - py * t.sin + t.shift_x, px * t.sin + py * t.cos + t.shift_y};
}

inline std::pair<float, float> ViewToWorld(const ViewTransform& t, float mx, float my) {
  const float dx = mx - t.shift_x, dy = my - t.shift_y;
  return {t.origin_x + (dx * t.cos + dy * t.sin) / t.scale, t.origin_z + (dy * t.cos - dx * t.sin) / t.scale};
}

// The map panel in the menu layout, measured from the city and district screens (the same panel in both). Dots
// outside it belong to parts of the city the view does not show.
inline constexpr float kPanelLeft = 403.0f;
inline constexpr float kPanelTop = 70.0f;
inline constexpr float kPanelRight = 879.0f;
inline constexpr float kPanelBottom = 563.0f;

// Written so a NaN position is outside.
constexpr bool InsidePanel(float mx, float my) {
  return mx >= kPanelLeft && mx <= kPanelRight && my >= kPanelTop && my <= kPanelBottom;
}

// The city's districts are islands. Each island's centre is the mean position of its orbs, in world units; an orb
// belongs to the district of the nearest centre. The Agency's island is small, so it only claims what lies within
// kAgencyReach of its centre.
enum class District { LosMuertos, Agency, NorthEast, SouthEast };

struct IslandCentre {
  float x, z;
  District district;
};

inline constexpr IslandCentre kIslandCentres[] = {
    {1045.0f, -1321.0f, District::LosMuertos},  // the southern island
    {1100.0f, -2711.0f, District::LosMuertos},  // the northern island
    {1961.0f, -1866.0f, District::Agency},
    {2318.0f, -2845.0f, District::NorthEast},
    {2654.0f, -1297.0f, District::SouthEast},  // its northern part
    {2427.0f, -428.0f, District::SouthEast},   // its southern part
    {3602.0f, -270.0f, District::SouthEast},   // the islet off its east coast
};
inline constexpr float kAgencyReach = 300.0f;

inline District DistrictAt(float x, float z) {
  District nearest = District::LosMuertos;
  float best = INFINITY;
  for (const IslandCentre& centre : kIslandCentres) {
    const float distance = std::hypot(x - centre.x, z - centre.z);
    if (centre.district == District::Agency && distance > kAgencyReach) {
      continue;
    }
    if (distance < best) {
      best = distance;
      nearest = centre.district;
    }
  }
  return nearest;
}

// --- Drawing -----------------------------------------------------------------------------------------------------

// The HUD and the menus are laid out 1280 x 720 and stretched across the canvas, which is 720 lines tall with the
// video mode's shape. The canvas fills the window when stretched, else keeps its shape in the middle of it.
inline constexpr float kLayoutWidth = 1280.0f;
inline constexpr float kLayoutHeight = 720.0f;

struct LayoutToWindow {
  float left = 0.0f, top = 0.0f;        // where the layout's (0, 0) lands
  float scale_x = 0.0f, scale_y = 0.0f;  // window pixels per layout unit
};

inline LayoutToWindow MapLayoutToWindow(float window_width, float window_height, float mode_width, float mode_height,
                                        bool letterbox) {
  LayoutToWindow m;
  if (!(mode_width > 0.0f) || !(mode_height > 0.0f)) {
    return m;
  }
  const float canvas_height = kLayoutHeight;
  const float canvas_width = canvas_height * mode_width / mode_height;
  float sx = window_width / canvas_width;
  float sy = window_height / canvas_height;
  if (letterbox) {
    sx = sy = std::min(sx, sy);
  }
  m.left = (window_width - canvas_width * sx) * 0.5f;
  m.top = (window_height - canvas_height * sy) * 0.5f;
  m.scale_x = sx * canvas_width / kLayoutWidth;
  m.scale_y = sy * canvas_height / kLayoutHeight;
  return m;
}

}  // namespace crackdown::orb_map
