#pragma once

#include "screen.h"
#include <vector>
#include <string>

namespace esphome {
namespace m5dial_sonos {

// ── Menu screen ───────────────────────────────────────────────────────────────
//
// Shows a scrollable carousel of group names. The orchestrator drives
// selection via on_rotary_cw/ccw and reads the result with get_selection().
// Short press and long press are handled entirely by the orchestrator.
//
// Open: double-tap the touchscreen.
// Select: short press the button.
// Cancel: long press the button.

class MenuScreen : public Screen {
 public:
  // Call once after all groups are registered (or whenever the group list changes).
  void set_groups(const std::vector<std::string> &names);

  // Sync highlighted item when re-opening the menu.
  void set_selection(size_t idx);
  size_t get_selection() const { return selection_; }

  void draw()          override;
  void on_rotary_cw()  override;
  void on_rotary_ccw() override;

 private:
  std::vector<std::string> names_;
  size_t count_    {0};
  size_t selection_{0};
};

}  // namespace m5dial_sonos
}  // namespace esphome
