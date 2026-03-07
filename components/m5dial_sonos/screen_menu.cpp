#include "screen_menu.h"
#include <M5Dial.h>

namespace esphome {
namespace m5dial_sonos {

void MenuScreen::set_groups(const std::vector<std::string> &names) {
  names_     = names;
  count_     = names.size();
  selection_ = 0;
  mark_dirty();
}

void MenuScreen::set_selection(size_t idx) {
  if (idx < count_) {
    selection_ = idx;
    mark_dirty();
  }
}

void MenuScreen::on_rotary_cw() {
  if (count_ == 0) return;
  selection_ = (selection_ + 1) % count_;
  mark_dirty();
}

void MenuScreen::on_rotary_ccw() {
  if (count_ == 0) return;
  selection_ = (selection_ - 1 + count_) % count_;
  mark_dirty();
}

void MenuScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // ── Header ────────────────────────────────────────────────────────────────
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.drawString("MENU", CENTER_X, 28);
  dsp.drawFastHLine(CENTER_X - 28, 42, 56, COL_GREY_44);

  if (count_ == 0) {
    dsp.setTextColor(COL_GREY_55, COL_BG);
    dsp.setFont(&fonts::FreeSansBold9pt7b);
    dsp.drawString("No screens", CENTER_X, CENTER_Y);
    return;
  }

  // ── Carousel (prev / current / next) ─────────────────────────────────────
  int n    = static_cast<int>(count_);
  int prev = (static_cast<int>(selection_) - 1 + n) % n;
  int next = (static_cast<int>(selection_) + 1) % n;

  screen_draw_clipped(CENTER_X, CENTER_Y - 44,
                      names_[prev], 160,
                      COL_GREY_55, &fonts::FreeSansBold9pt7b);

  screen_draw_clipped(CENTER_X, CENTER_Y,
                      names_[selection_], 200,
                      COL_WHITE, &fonts::FreeSansBold12pt7b);

  screen_draw_clipped(CENTER_X, CENTER_Y + 44,
                      names_[next], 160,
                      COL_GREY_55, &fonts::FreeSansBold9pt7b);

  // ── Hint: short press to select ───────────────────────────────────────────
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.setTextColor(COL_GREY_44, COL_BG);
  dsp.drawString("press to select", CENTER_X, 185);

  // ── Selection dots ────────────────────────────────────────────────────────
  if (count_ > 1) {
    const int dot_r       = 3;
    const int dot_spacing = 12;
    const int dot_y       = 212;
    int total_w  = (n - 1) * dot_spacing;
    int x_start  = CENTER_X - total_w / 2;
    for (int i = 0; i < n; i++) {
      uint16_t c = (static_cast<size_t>(i) == selection_) ? COL_ORANGE : COL_GREY_44;
      dsp.fillCircle(x_start + i * dot_spacing, dot_y, dot_r, c);
    }
  }
}

}  // namespace m5dial_sonos
}  // namespace esphome
