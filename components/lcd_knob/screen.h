#pragma once

#include <string>
#include <cstdint>

namespace esphome {
namespace lcd_knob {

// ── Shared RGB565 colour palette ──────────────────────────────────────────────
static constexpr uint16_t COL_BG      = 0x18E3;  // #1A1A1A dark background
static constexpr uint16_t COL_ORANGE  = 0xFCA0;  // #FF9500 accent
static constexpr uint16_t COL_WHITE   = 0xFFFF;
static constexpr uint16_t COL_GREY_CC = 0xCE79;  // #CCCCCC
static constexpr uint16_t COL_GREY_55 = 0x52AA;  // #555555
static constexpr uint16_t COL_GREY_44 = 0x4228;  // #444444
static constexpr uint16_t COL_GREY_33 = 0x31A6;  // #333333

// ── Display geometry ──────────────────────────────────────────────────────────
static constexpr int CENTER_X = 120;
static constexpr int CENTER_Y = 120;

// ── Shared drawing helpers ────────────────────────────────────────────────────
// Draw text centred at (x, y), truncating with "..." if wider than max_width px.
// Uses COL_BG as text background (for screens with solid dark bg).
void screen_draw_clipped(int32_t x, int32_t y, const std::string &text,
                         int max_width, uint32_t color, const void *font);

// Return text clipped to max_width pixels (with "..." if truncated).
// Measures with M5Dial.Display — does NOT draw anything.
std::string screen_clip_to_width(const std::string &text, int max_width,
                                 const void *font);

// ── Abstract screen base ──────────────────────────────────────────────────────
class Screen {
 public:
  virtual ~Screen() = default;

  // Render full screen content. Called only when dirty.
  virtual void draw() = 0;

  // Input events — override what the screen needs.
  virtual void on_rotary_cw()    {}
  virtual void on_rotary_ccw()   {}
  virtual void on_short_press()  {}

  // Long press: return true if consumed here (stay on this screen).
  // Return false and the orchestrator advances to the next screen.
  virtual bool on_long_press()   { return false; }

  void mark_dirty()           { dirty_ = true; }
  bool is_dirty()  const      { return dirty_;  }
  void clear_dirty()          { dirty_ = false; }

 protected:
  bool dirty_{true};
};

}  // namespace lcd_knob
}  // namespace esphome
