#include "screen_countup.h"
#include <M5Dial.h>
#include <cstdio>

namespace esphome {
namespace lcd_knob {

// ── tick ──────────────────────────────────────────────────────────────────────

void CountUpScreen::tick(uint32_t now_ms) {
  if (!state_->running) return;

  uint32_t elapsed = (now_ms - state_->start_ms) / 1000;
  if (elapsed != last_elapsed_s_) {
    last_elapsed_s_  = elapsed;
    state_->elapsed_s = elapsed;
    mark_dirty();
  }
}

// ── Button ────────────────────────────────────────────────────────────────────

void CountUpScreen::on_short_press() {
  if (!state_->running) {
    state_->start_ms = millis() - state_->elapsed_s * 1000;
    state_->running  = true;
    mark_dirty();
  }
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void CountUpScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── Label ────────────────────────────────────────────────────────────────
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_SMALL);
  dsp.drawString("COUNT UP", CENTER_X, 28);

  // ── Elapsed time ─────────────────────────────────────────────────────────
  uint32_t elapsed = state_->elapsed_s;
  char buf[12];

  dsp.setTextColor(COL_WHITE, COL_BG);

  if (elapsed < 3600) {
    uint32_t mm = elapsed / 60;
    uint32_t ss = elapsed % 60;
    snprintf(buf, sizeof(buf), "%02u:%02u", mm, ss);
    dsp.setFont(FONT_LARGE);
    dsp.drawString(buf, CENTER_X, CENTER_Y - 8);
  } else {
    uint32_t hh = elapsed / 3600;
    uint32_t mm = (elapsed % 3600) / 60;
    uint32_t ss = elapsed % 60;
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hh, mm, ss);
    dsp.setFont(FONT_MEDIUM);
    dsp.drawString(buf, CENTER_X, CENTER_Y - 8);
  }

  // ── Status line ──────────────────────────────────────────────────────────
  dsp.setFont(FONT_SMALL);
  if (state_->running) {
    dsp.setTextColor(COL_ORANGE, COL_BG);
    dsp.drawString("RUNNING", CENTER_X, 160);
  } else {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.drawString("STOPPED", CENTER_X, 160);
  }
}

}  // namespace lcd_knob
}  // namespace esphome
