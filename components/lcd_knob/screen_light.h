#pragma once

#include "screen.h"
#include <string>

namespace esphome {
namespace lcd_knob {

// ── Per-light state ────────────────────────────────────────────────────────────
// Owned by LcdKnob; LightScreen holds a pointer to it.
struct LightState {
  std::string entity;
  std::string name;
  bool    is_on{false};
  uint8_t brightness{0};   // 0-100 %
  bool    pending{false};  // device-side change waiting to be sent to HA
};

// ── Light brightness / on-off screen ─────────────────────────────────────────
// Arc ring = brightness level (like SonosVolumeScreen).
// Rotary CW/CCW adjusts brightness (turning up from 0 also turns the light on;
// turning down to 0 turns it off).
// Short press toggles on / off.
// Long press falls through so the orchestrator cycles to the next light page.
class LightScreen : public Screen {
 public:
  explicit LightScreen(LightState *state) : state_(state) {}

  void draw()           override;
  void on_rotary_cw()   override;
  void on_rotary_ccw()  override;
  void on_short_press() override;

  const LightState *light_state() const { return state_; }

 private:
  LightState *state_;
  static constexpr uint8_t STEP = 5;  // brightness step per encoder click
};

}  // namespace lcd_knob
}  // namespace esphome
