#include "screen_light.h"
#include <M5Dial.h>
#include <algorithm>
#include <cstdio>

namespace esphome {
namespace lcd_knob {

// ── Input ──────────────────────────────────────────────────────────────────────

void LightScreen::on_rotary_cw() {
  if (state_->brightness >= 100) return;
  state_->brightness = (uint8_t)std::min((int)state_->brightness + STEP, 100);
  state_->is_on      = true;   // turning up from 0 turns the light on
  state_->pending    = true;
  mark_dirty();
}

void LightScreen::on_rotary_ccw() {
  if (state_->brightness == 0) return;
  state_->brightness = (state_->brightness >= STEP) ? state_->brightness - STEP : 0;
  if (state_->brightness == 0) state_->is_on = false;  // dim to off
  state_->pending = true;
  mark_dirty();
}

void LightScreen::on_short_press() {
  state_->is_on = !state_->is_on;
  if (state_->is_on && state_->brightness == 0)
    state_->brightness = 50;  // sensible default when turning on from 0
  state_->pending = true;
  mark_dirty();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void LightScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── Label ────────────────────────────────────────────────────────────────
  screen_draw_clipped(CENTER_X, 28, state_->name, 180, COL_ORANGE, FONT_SMALL);

  // ── Arc ring ─────────────────────────────────────────────────────────────
  // Background: full 270° sweep (gap at bottom)
  dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
              ARC_START, ARC_END_FULL, COL_GREY_33);

  if (state_->is_on && state_->brightness > 0) {
    float end_angle = ARC_START + ARC_SWEEP * state_->brightness / 100.0f;
    dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
                ARC_START, (int)end_angle, COL_ORANGE);
  }

  // ── Centre text ───────────────────────────────────────────────────────────
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_LARGE);

  if (state_->is_on) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", state_->brightness);
    dsp.setTextColor(COL_WHITE, COL_BG);
    dsp.drawString(buf, CENTER_X, CENTER_Y - 8);

    dsp.setFont(FONT_SMALL);
    dsp.setTextColor(COL_ORANGE, COL_BG);
    dsp.drawString("ON", CENTER_X, 162);
  } else {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.drawString("OFF", CENTER_X, CENTER_Y - 8);
  }
}

}  // namespace lcd_knob
}  // namespace esphome
