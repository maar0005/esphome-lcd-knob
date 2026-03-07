#include "screen_sonos.h"
#include <M5Dial.h>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace lcd_knob {

// ── Off-screen canvas for compositing album art ───────────────────────────────
// Allocated once on first draw; lives in PSRAM (240×240×2 = ~113 KB).
static M5Canvas *s_art_canvas = nullptr;

// ═══════════════════════════════════════════════════════════════════════════════
// SonosPlaylistScreen
// ═══════════════════════════════════════════════════════════════════════════════

void SonosPlaylistScreen::on_rotary_cw() {
  if (state_->playlist_names.empty()) return;
  int n = (int)state_->playlist_names.size();
  state_->playlist_index = (state_->playlist_index + 1) % n;
  mark_dirty();
}

void SonosPlaylistScreen::on_rotary_ccw() {
  if (state_->playlist_names.empty()) return;
  int n = (int)state_->playlist_names.size();
  state_->playlist_index = (state_->playlist_index - 1 + n) % n;
  mark_dirty();
}

void SonosPlaylistScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // Subtle outer ring
  dsp.drawArc(CENTER_X, CENTER_Y, 119, 117, 0, 360, COL_GREY_33);

  // Header
  dsp.setTextDatum(middle_center);
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.drawString("FAVOURITES", CENTER_X, 24);

  if (state_->playlist_names.empty()) {
    dsp.setFont(&fonts::FreeSansBold12pt7b);
    dsp.setTextColor(COL_WHITE, COL_BG);
    dsp.drawString("No favourites", CENTER_X, CENTER_Y);
    return;
  }

  int n    = (int)state_->playlist_names.size();
  int cur  = state_->playlist_index;
  int prev = (cur - 1 + n) % n;
  int next = (cur + 1    ) % n;

  // Dim items above and below
  screen_draw_clipped(CENTER_X, CENTER_Y - 44,
                      state_->playlist_names[prev], 160,
                      COL_GREY_55, &fonts::FreeSansBold9pt7b);
  screen_draw_clipped(CENTER_X, CENTER_Y + 44,
                      state_->playlist_names[next], 160,
                      COL_GREY_55, &fonts::FreeSansBold9pt7b);

  // Orange left accent bar for selected item
  dsp.fillRoundRect(14, CENTER_Y - 17, 4, 34, 2, COL_ORANGE);

  // Selected item (slightly right of centre to clear the accent bar)
  screen_draw_clipped(CENTER_X + 3, CENTER_Y,
                      state_->playlist_names[cur], 190,
                      COL_WHITE, &fonts::FreeSansBold12pt7b);

  // Position counter  "3 / 12"
  char buf[14];
  snprintf(buf, sizeof(buf), "%d / %d", cur + 1, n);
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.setTextColor(COL_GREY_44, COL_BG);
  dsp.drawString(buf, CENTER_X, 183);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SonosNowPlayingScreen — helpers
// ═══════════════════════════════════════════════════════════════════════════════

// Draw play/pause indicator into a canvas at top-centre.
static void draw_play_pause(M5Canvas &c, bool is_playing) {
  const int cx = CENTER_X, cy = 30;
  if (is_playing) {
    c.fillRect(cx - 11, cy - 10, 7, 20, COL_ORANGE);
    c.fillRect(cx +  4, cy - 10, 7, 20, COL_ORANGE);
  } else {
    c.fillTriangle(cx - 10, cy - 11, cx - 10, cy + 11, cx + 12, cy, COL_ORANGE);
  }
}

// Draw text with 1 px drop-shadow onto a canvas (transparent background).
static void canvas_text_shadow(M5Canvas &c, int32_t x, int32_t y,
                                const std::string &t, uint32_t color) {
  c.setTextDatum(middle_center);
  c.setTextColor(0x0000);          // black shadow
  c.drawString(t.c_str(), x + 1, y + 1);
  c.setTextColor(color);           // main text (transparent bg)
  c.drawString(t.c_str(), x, y);
}

// ── Art mode ──────────────────────────────────────────────────────────────────

void SonosNowPlayingScreen::draw_art_mode_() {
  auto &dsp = M5Dial.Display;

  // Lazy-create the compositing canvas (lives for the lifetime of the firmware)
  if (!s_art_canvas) {
    s_art_canvas = new M5Canvas(&dsp);
    s_art_canvas->setColorDepth(16);
    s_art_canvas->createSprite(240, 240);
  }

  // 1. Decode and scale album art into the canvas
  s_art_canvas->drawJpg(state_->album_art_data.data(),
                         state_->album_art_data.size(),
                         0, 0, 240, 240);

  // 2. Gradient overlay: rows 148–235 fade to black
  //    Pixel-level blend toward black — fast in PSRAM (~15–25 ms)
  for (int y = 148; y < 236; y++) {
    const uint8_t blend   = (uint8_t)((y - 148) * 255 / 87);
    const uint8_t inv     = 255 - blend;
    for (int x = 0; x < 240; x++) {
      uint16_t px = s_art_canvas->readPixel(x, y);
      uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * inv / 255);
      uint8_t g = (uint8_t)(((px >>  5) & 0x3F) * inv / 255);
      uint8_t b = (uint8_t)( (px        & 0x1F) * inv / 255);
      s_art_canvas->writePixel(x, y, (uint16_t)((r << 11) | (g << 5) | b));
    }
  }
  // Solid black strip at very bottom for text contrast
  s_art_canvas->fillRect(0, 236, 240, 4, 0x0000);

  // 3. Play / pause icon
  draw_play_pause(*s_art_canvas, state_->is_playing);

  // 4. Track title with shadow
  s_art_canvas->setFont(&fonts::FreeSansBold12pt7b);
  auto title = screen_clip_to_width(state_->media_title, 200,
                                     &fonts::FreeSansBold12pt7b);
  canvas_text_shadow(*s_art_canvas, CENTER_X, 188, title, COL_WHITE);

  // 5. Artist with shadow
  s_art_canvas->setFont(&fonts::FreeSansBold9pt7b);
  auto artist = screen_clip_to_width(state_->media_artist, 192,
                                      &fonts::FreeSansBold9pt7b);
  canvas_text_shadow(*s_art_canvas, CENTER_X, 213, artist, COL_ORANGE);

  // 6. Push canvas to display in one DMA burst
  s_art_canvas->pushSprite(0, 0);
}

// ── Fallback mode (no art yet) ────────────────────────────────────────────────

void SonosNowPlayingScreen::draw_fallback_mode_() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // Play / pause icon
  if (state_->is_playing) {
    dsp.fillRect(CENTER_X - 10, 18, 6, 20, COL_ORANGE);
    dsp.fillRect(CENTER_X +  4, 18, 6, 20, COL_ORANGE);
  } else {
    dsp.fillTriangle(CENTER_X - 8, 17, CENTER_X - 8, 38,
                     CENTER_X + 10, 27, COL_ORANGE);
  }

  // Music bars — heights seeded by track title so every song looks different
  uint32_t seed = 0;
  for (char c : state_->media_title) seed = seed * 31 + (uint8_t)c;
  const int N = 11, BW = 10, SP = 5;
  const int BX = CENTER_X - (N * (BW + SP) - SP) / 2;
  const uint16_t bc = state_->is_playing ? COL_ORANGE : COL_GREY_33;
  for (int i = 0; i < N; i++) {
    seed = seed * 1664525u + 1013904223u;
    int h = 14 + (int)((seed >> 24) % 42);
    dsp.fillRoundRect(BX + i * (BW + SP), 92 - h / 2, BW, h, 3, bc);
  }

  // Track info
  screen_draw_clipped(CENTER_X, 148, state_->media_title, 200,
                      COL_WHITE,   &fonts::FreeSansBold12pt7b);
  screen_draw_clipped(CENTER_X, 174, state_->media_artist, 192,
                      COL_GREY_CC, &fonts::FreeSansBold9pt7b);
}

// ── Public draw ───────────────────────────────────────────────────────────────

void SonosNowPlayingScreen::draw() {
  if (state_->album_art_valid && !state_->album_art_data.empty())
    draw_art_mode_();
  else
    draw_fallback_mode_();
}

// ═══════════════════════════════════════════════════════════════════════════════
// SonosVolumeScreen
// ═══════════════════════════════════════════════════════════════════════════════

void SonosVolumeScreen::on_rotary_cw() {
  state_->volume += volume_step_ / 100.0f;
  if (state_->volume > 1.0f) state_->volume = 1.0f;
  mark_dirty();
}

void SonosVolumeScreen::on_rotary_ccw() {
  state_->volume -= volume_step_ / 100.0f;
  if (state_->volume < 0.0f) state_->volume = 0.0f;
  mark_dirty();
}

void SonosVolumeScreen::draw() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  const int pct = (int)(state_->volume * 100.0f + 0.5f);

  // Header
  dsp.setTextDatum(middle_center);
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.drawString("VOLUME", CENTER_X, 28);

  // Background arc  135° → 405°  (270° sweep, gap at bottom)
  dsp.fillArc(CENTER_X, CENTER_Y, 108, 96, 135, 405, COL_GREY_33);

  if (pct > 0) {
    // Foreground arc
    float end_angle = 135.0f + 270.0f * pct / 100.0f;
    dsp.fillArc(CENTER_X, CENTER_Y, 108, 96, 135, (int)end_angle, COL_ORANGE);

    // White cap dot at arc tip
    float rad = end_angle * (float)M_PI / 180.0f;
    int   dx  = CENTER_X + (int)(102.0f * cosf(rad));
    int   dy  = CENTER_Y + (int)(102.0f * sinf(rad));
    dsp.fillCircle(dx, dy, 7, COL_BG);    // dark ring
    dsp.fillCircle(dx, dy, 5, COL_WHITE); // white cap
  }

  // Volume percentage — large centred text
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  dsp.setFont(&fonts::FreeSansBold18pt7b);
  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.drawString(buf, CENTER_X, CENTER_Y);
}

}  // namespace lcd_knob
}  // namespace esphome
