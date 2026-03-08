#include "screen_sonos.h"
#include <M5Dial.h>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace lcd_knob {

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
  dsp.drawArc(CENTER_X, CENTER_Y, ARC_DECO_OUTER, ARC_DECO_INNER, 0, 360, COL_GREY_33);

  // Header
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_SMALL);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.drawString("FAVOURITES", CENTER_X, 24);

  if (state_->playlist_names.empty()) {
    dsp.setFont(FONT_MEDIUM);
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
                      COL_GREY_55, FONT_SMALL);
  screen_draw_clipped(CENTER_X, CENTER_Y + 44,
                      state_->playlist_names[next], 160,
                      COL_GREY_55, FONT_SMALL);

  // Orange left accent bar for selected item
  dsp.fillRoundRect(14, CENTER_Y - 17, 4, 34, 2, COL_ORANGE);

  // Selected item (slightly right of centre to clear the accent bar)
  screen_draw_clipped(CENTER_X + 3, CENTER_Y,
                      state_->playlist_names[cur], 190,
                      COL_WHITE, FONT_MEDIUM);

  // Position counter  "3 / 12"
  char buf[14];
  snprintf(buf, sizeof(buf), "%d / %d", cur + 1, n);
  dsp.setFont(FONT_SMALL);
  dsp.setTextColor(COL_GREY_44, COL_BG);
  dsp.drawString(buf, CENTER_X, 183);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SonosNowPlayingScreen
// ═══════════════════════════════════════════════════════════════════════════════

// ── Art mode — drawn directly to display, no off-screen buffer needed ─────────

void SonosNowPlayingScreen::draw_art_mode_() {
  auto &dsp = M5Dial.Display;

  // 1. Decode and scale JPEG directly onto the display (no canvas required)
  dsp.drawJpg(state_->album_art_data.data(),
               state_->album_art_data.size(),
               0, 0, 240, 240);

  // 2. Solid dark bar at bottom for text contrast — covers rows 168–239
  dsp.fillRect(0, 168, 240, 72, COL_BLACK);
  dsp.drawFastHLine(0, 168, 240, COL_GREY_33);  // subtle top edge

  // 3. Play / pause icon — dark halo first so it reads over any art colour
  dsp.fillCircle(CENTER_X, 30, 17, COL_BLACK);
  if (state_->is_playing) {
    dsp.fillRect(CENTER_X - 10, 20, 7, 20, COL_ORANGE);
    dsp.fillRect(CENTER_X +  3, 20, 7, 20, COL_ORANGE);
  } else {
    dsp.fillTriangle(CENTER_X - 9, 19, CENTER_X - 9, 40,
                     CENTER_X + 11, 29, COL_ORANGE);
  }

  // 4. Track title in the dark bar
  dsp.setTextDatum(middle_center);
  dsp.setFont(FONT_MEDIUM);
  dsp.setTextColor(COL_WHITE, COL_BLACK);
  auto title = screen_clip_to_width(state_->media_title, 200, FONT_MEDIUM);
  dsp.drawString(title.c_str(), CENTER_X, 190);

  // 5. Artist below title
  dsp.setFont(FONT_SMALL);
  dsp.setTextColor(COL_ORANGE, COL_BLACK);
  auto artist = screen_clip_to_width(state_->media_artist, 192, FONT_SMALL);
  dsp.drawString(artist.c_str(), CENTER_X, 214);
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
                      COL_WHITE,   FONT_MEDIUM);
  screen_draw_clipped(CENTER_X, 174, state_->media_artist, 192,
                      COL_GREY_CC, FONT_SMALL);
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
  dsp.setFont(FONT_SMALL);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.drawString("VOLUME", CENTER_X, 28);

  // Background arc  ARC_START → ARC_END_FULL  (270° sweep, gap at bottom)
  dsp.fillArc(CENTER_X, CENTER_Y, ARC_VOL_OUTER, ARC_VOL_INNER,
              ARC_START, ARC_END_FULL, COL_GREY_33);

  if (pct > 0) {
    // Foreground arc
    float end_angle = ARC_START + ARC_SWEEP * pct / 100.0f;
    dsp.fillArc(CENTER_X, CENTER_Y, ARC_VOL_OUTER, ARC_VOL_INNER,
                ARC_START, (int)end_angle, COL_ORANGE);

    // Cap dot at arc tip
    float rad = end_angle * (float)M_PI / 180.0f;
    int   dx  = CENTER_X + (int)(ARC_CAP_DIST * cosf(rad));
    int   dy  = CENTER_Y + (int)(ARC_CAP_DIST * sinf(rad));
    dsp.fillCircle(dx, dy, ARC_CAP_HALO, COL_BG);    // dark halo
    dsp.fillCircle(dx, dy, ARC_CAP_DOT,  COL_WHITE); // bright tip
  }

  // Volume percentage — large centred text
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  dsp.setFont(FONT_LARGE);
  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.drawString(buf, CENTER_X, CENTER_Y);
}

}  // namespace lcd_knob
}  // namespace esphome
