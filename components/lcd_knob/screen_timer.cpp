#include "screen_timer.h"
#include <M5Dial.h>
#include <cstdio>
#include <algorithm>

namespace esphome {
namespace lcd_knob {

// ── tick ──────────────────────────────────────────────────────────────────────

void TimerScreen::tick(uint32_t now_ms) {
  if (!state_->running) return;

  uint32_t elapsed = (now_ms - state_->start_ms) / 1000 + state_->elapsed_at_pause;

  if (elapsed >= state_->duration_s) {
    state_->running         = false;
    state_->remaining_s     = 0;
    state_->elapsed_at_pause = state_->duration_s;
    state_->fired           = true;
    mark_dirty();
    return;
  }

  uint32_t new_remaining = state_->duration_s - elapsed;
  if (new_remaining != state_->remaining_s) {
    state_->remaining_s = new_remaining;
    mark_dirty();
  }
}

// ── Rotary ────────────────────────────────────────────────────────────────────

void TimerScreen::adjust_duration(int direction) {
  if (state_->running) return;  // no adjustment while running

  uint32_t now = millis();
  if (now - last_rotary_ms_ < 300) {
    rotary_streak_++;
  } else {
    rotary_streak_ = 0;
  }
  last_rotary_ms_ = now;

  uint32_t step = 30 * std::max(1, rotary_streak_ / 3);
  if (step > 300) step = 300;

  if (direction > 0) {
    uint32_t max_s = 99 * 60 - 1;  // 98:59
    state_->duration_s = std::min(state_->duration_s + step, max_s);
  } else {
    if (state_->duration_s > step)
      state_->duration_s -= step;
    else
      state_->duration_s = 30;  // minimum 30 s
  }

  state_->remaining_s     = state_->duration_s;
  state_->elapsed_at_pause = 0;
  state_->fired           = false;  // clear fired state when user sets a new duration
  mark_dirty();
}

void TimerScreen::on_rotary_cw()  { adjust_duration(+1); }
void TimerScreen::on_rotary_ccw() { adjust_duration(-1); }

// ── Button ────────────────────────────────────────────────────────────────────

void TimerScreen::on_short_press() {
  uint32_t now = millis();

  if (state_->fired) {
    // Clear fired state and reset on press while fired
    state_->fired           = false;
    state_->running         = false;
    state_->remaining_s     = state_->duration_s;
    state_->elapsed_at_pause = 0;
    mark_dirty();
    return;
  }

  if (state_->running) {
    // Pause: snapshot elapsed
    uint32_t elapsed = (now - state_->start_ms) / 1000;
    state_->elapsed_at_pause += elapsed;
    state_->running = false;
  } else {
    // Start / Resume
    if (state_->remaining_s == 0) {
      // Restart from full duration
      state_->remaining_s     = state_->duration_s;
      state_->elapsed_at_pause = 0;
    }
    state_->start_ms = now;
    state_->running  = true;
  }
  mark_dirty();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void TimerScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── Label ────────────────────────────────────────────────────────────────
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_SMALL);
  dsp.drawString(label_, CENTER_X, 28);

  // ── Background arc (full ring) ───────────────────────────────────────────
  dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
              ARC_START, ARC_END_FULL, COL_GREY_33);

  // ── Foreground arc (proportional to remaining / duration) ───────────────
  uint16_t arc_colour = state_->fired ? COL_WHITE : COL_ORANGE;
  float pct = 0.0f;
  if (state_->duration_s > 0) {
    pct = (float)state_->remaining_s / (float)state_->duration_s;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
  }

  if (pct > 0.0f) {
    float end_angle = ARC_START + ARC_SWEEP * pct;
    dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
                ARC_START, (int)end_angle, arc_colour);
  }

  // ── Time display (MM:SS) ─────────────────────────────────────────────────
  uint32_t display_s = state_->remaining_s;
  uint32_t mm = display_s / 60;
  uint32_t ss = display_s % 60;

  char buf[8];
  snprintf(buf, sizeof(buf), "%02u:%02u", mm, ss);

  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.setFont(FONT_LARGE);
  dsp.drawString(buf, CENTER_X, CENTER_Y - 8);

  // ── Status line ──────────────────────────────────────────────────────────
  dsp.setFont(FONT_SMALL);
  if (state_->fired) {
    dsp.setTextColor(COL_WHITE, COL_BG);
    dsp.drawString("DONE", CENTER_X, 160);
  } else if (!state_->running) {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.drawString("PAUSED", CENTER_X, 160);
  }
}

}  // namespace lcd_knob
}  // namespace esphome
