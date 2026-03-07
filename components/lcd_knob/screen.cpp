#include "screen.h"
#include <M5Dial.h>

namespace esphome {
namespace lcd_knob {

void screen_draw_clipped(int32_t x, int32_t y, const std::string &text,
                         int max_width, uint32_t color, const void *font) {
  auto &dsp = M5Dial.Display;
  dsp.setFont(static_cast<const lgfx::GFXfont *>(font));
  dsp.setTextColor(color, COL_BG);
  dsp.setTextDatum(middle_center);

  int tw = dsp.textWidth(text.c_str());
  if (tw <= max_width) {
    dsp.drawString(text.c_str(), x, y);
    return;
  }

  std::string t = text;
  int ew = dsp.textWidth("...");
  while (t.size() > 1 && dsp.textWidth(t.c_str()) + ew > max_width)
    t.pop_back();
  t += "...";
  dsp.drawString(t.c_str(), x, y);
}

std::string screen_clip_to_width(const std::string &text, int max_width,
                                 const void *font) {
  auto &dsp = M5Dial.Display;
  dsp.setFont(static_cast<const lgfx::GFXfont *>(font));
  if (dsp.textWidth(text.c_str()) <= max_width) return text;
  std::string t = text;
  int ew = dsp.textWidth("...");
  while (t.size() > 1 && dsp.textWidth(t.c_str()) + ew > max_width)
    t.pop_back();
  return t + "...";
}

}  // namespace lcd_knob
}  // namespace esphome
