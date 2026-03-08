#include "screen_alarm.h"
#include <M5Dial.h>
#include <cstdio>

namespace esphome {
namespace lcd_knob {

// ── Input ─────────────────────────────────────────────────────────────────────

void AlarmScreen::on_rotary_cw() {
  switch (state_->edit_field) {
    case AlarmEditField::HOUR:
      state_->hour = (state_->hour + 1) % 24;
      mark_dirty();
      break;
    case AlarmEditField::MINUTE:
      state_->minute = (state_->minute + 1) % 60;
      mark_dirty();
      break;
    default:
      break;
  }
}

void AlarmScreen::on_rotary_ccw() {
  switch (state_->edit_field) {
    case AlarmEditField::HOUR:
      state_->hour = (state_->hour + 23) % 24;
      mark_dirty();
      break;
    case AlarmEditField::MINUTE:
      state_->minute = (state_->minute + 59) % 60;
      mark_dirty();
      break;
    default:
      break;
  }
}

void AlarmScreen::on_short_press() {
  switch (state_->edit_field) {
    case AlarmEditField::NONE:
      state_->edit_field = AlarmEditField::HOUR;
      blink_ms_ = millis();
      blink_on_ = true;
      break;
    case AlarmEditField::HOUR:
      state_->edit_field = AlarmEditField::MINUTE;
      break;
    case AlarmEditField::MINUTE:
      state_->edit_field = AlarmEditField::NONE;
      state_->armed      = true;
      state_->fired      = false;
      break;
  }
  mark_dirty();
}

bool AlarmScreen::on_long_press() {
  if (state_->edit_field != AlarmEditField::NONE) {
    // Cancel edit without saving — stay on this screen
    state_->edit_field = AlarmEditField::NONE;
    blink_on_          = true;
    mark_dirty();
    return true;
  }
  // Not editing: let the orchestrator cycle to the next page
  return false;
}

// ── Alarm check (called from LcdKnob::check_alarms every minute) ─────────────

void AlarmScreen::check(uint8_t h, uint8_t m) {
  if (state_->armed && !state_->fired &&
      h == state_->hour && m == state_->minute) {
    state_->fired = true;
    mark_dirty();
  }
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void AlarmScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── Label ────────────────────────────────────────────────────────────────
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_SMALL);
  dsp.drawString(label_, CENTER_X, 28);

  // ── Blink logic ──────────────────────────────────────────────────────────
  bool in_edit = (state_->edit_field != AlarmEditField::NONE);
  if (in_edit) {
    uint32_t now = millis();
    if (now - blink_ms_ >= 500) {
      blink_on_ = !blink_on_;
      blink_ms_ = now;
    }
  } else {
    blink_on_ = true;
  }

  // ── Large HH:MM ──────────────────────────────────────────────────────────
  char hbuf[4], mbuf[4];
  snprintf(hbuf, sizeof(hbuf), "%02u", state_->hour);
  snprintf(mbuf, sizeof(mbuf), "%02u", state_->minute);

  dsp.setFont(FONT_LARGE);

  // Hour segment
  uint16_t h_colour = COL_WHITE;
  if (state_->edit_field == AlarmEditField::HOUR && !blink_on_)
    h_colour = COL_BG;  // blink off
  dsp.setTextColor(h_colour, COL_BG);
  dsp.drawString(hbuf, CENTER_X - 34, CENTER_Y - 8);

  // Colon (always visible)
  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.drawString(":", CENTER_X, CENTER_Y - 8);

  // Minute segment
  uint16_t m_colour = COL_WHITE;
  if (state_->edit_field == AlarmEditField::MINUTE && !blink_on_)
    m_colour = COL_BG;  // blink off
  dsp.setTextColor(m_colour, COL_BG);
  dsp.drawString(mbuf, CENTER_X + 34, CENTER_Y - 8);

  // ── Status line ──────────────────────────────────────────────────────────
  dsp.setFont(FONT_SMALL);
  if (state_->fired) {
    dsp.setTextColor(COL_WHITE, COL_BG);
    dsp.drawString("ALARM!", CENTER_X, 160);
  } else if (state_->armed) {
    dsp.setTextColor(COL_ORANGE, COL_BG);
    dsp.drawString("ARMED", CENTER_X, 160);
  } else if (in_edit) {
    dsp.setTextColor(COL_GREY_CC, COL_BG);
    if (state_->edit_field == AlarmEditField::HOUR)
      dsp.drawString("SET HOUR", CENTER_X, 160);
    else
      dsp.drawString("SET MIN", CENTER_X, 160);
  } else {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.drawString("DISARMED", CENTER_X, 160);
  }

  // ── Keep redrawing while blinking ────────────────────────────────────────
  if (in_edit) mark_dirty();
}

}  // namespace lcd_knob
}  // namespace esphome
