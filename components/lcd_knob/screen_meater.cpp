#include "screen_meater.h"
#include <M5Dial.h>

namespace esphome {
namespace lcd_knob {

void MeaterScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── No data yet ───────────────────────────────────────────────────────────
  if (!has_temp_) {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.setTextDatum(middle_center);
    dsp.setFont(FONT_MEDIUM);
    dsp.drawString("No probe", CENTER_X, CENTER_Y);
    return;
  }

  // ── Header ────────────────────────────────────────────────────────────────
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_SMALL);
  dsp.drawString("PROBE", CENTER_X, 28);

  // ── Progress arc (gap at bottom) ─────────────────────────────────────────
  // Shows how close the probe temp is to the target.
  dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
              ARC_START, ARC_END_FULL, COL_GREY_33);

  if (has_target_ && target_ > 0.0f) {
    float pct = temp_ / target_;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    // Colour shifts orange → white as it approaches target
    uint16_t arc_colour = (pct >= 0.95f) ? COL_WHITE : COL_ORANGE;

    if (pct > 0.0f) {
      float end_angle = ARC_START + ARC_SWEEP * pct;
      dsp.fillArc(CENTER_X, CENTER_Y, ARC_OUTER, ARC_INNER,
                  ARC_START, static_cast<int>(end_angle), arc_colour);
    }
  }

  // ── Current temperature (large, centred) ──────────────────────────────────
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f\xb0", temp_);  // e.g. "67°"
  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.setFont(FONT_LARGE);
  dsp.drawString(buf, CENTER_X, CENTER_Y - 8);

  // ── Target temperature ────────────────────────────────────────────────────
  if (has_target_) {
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "\x11 %.0f\xb0", target_);  // "→ 71°"
    dsp.setTextColor(COL_GREY_CC, COL_BG);
    dsp.setFont(FONT_SMALL);
    dsp.drawString(tbuf, CENTER_X, CENTER_Y + 28);
  }

  // ── Ambient temperature (small, at bottom) ───────────────────────────────
  if (ambient_ > 0.0f) {
    char abuf[24];
    snprintf(abuf, sizeof(abuf), "Amb %.0f\xb0", ambient_);
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.setFont(FONT_SMALL);
    dsp.drawString(abuf, CENTER_X, 195);
  }
}

}  // namespace lcd_knob
}  // namespace esphome
