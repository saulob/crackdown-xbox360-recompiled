#pragma once

namespace rex::ui {
class ImGuiDrawer;
}  // namespace rex::ui

namespace crackdown::orb_map {

// Adds the overlay that marks uncollected agility and hidden orbs on the minimap and the pause menu's map.
void CreateOverlay(rex::ui::ImGuiDrawer* drawer);

}  // namespace crackdown::orb_map
