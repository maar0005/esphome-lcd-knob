#include "m5dial_sonos.h"
#include "esphome/core/log.h"
#include <M5Dial.h>

namespace esphome {
namespace m5dial_sonos {

static const char *const TAG = "m5dial_sonos";

// ── RGB565 colours ───────────────────────────────────────────────────
static constexpr uint16_t COL_BG       = 0x18E3;  // #1A1A1A
static constexpr uint16_t COL_ORANGE   = 0xFCA0;  // #FF9500
static constexpr uint16_t COL_WHITE    = 0xFFFF;
static constexpr uint16_t COL_GREY_CC  = 0xCE79;  // #CCCCCC
static constexpr uint16_t COL_GREY_55  = 0x52AA;  // #555555
static constexpr uint16_t COL_GREY_44  = 0x4228;  // #444444
static constexpr uint16_t COL_GREY_33  = 0x31A6;  // #333333
static constexpr uint16_t COL_BLACK    = 0x0000;

// ── Display geometry ─────────────────────────────────────────────────
static constexpr int SCREEN_W  = 240;
static constexpr int SCREEN_H  = 240;
static constexpr int CENTER_X  = 120;
static constexpr int CENTER_Y  = 120;

// ── Brightness ───────────────────────────────────────────────────────
static constexpr uint8_t BRIGHTNESS_FULL = 100;
static constexpr uint8_t BRIGHTNESS_DIM  = 25;

// ═══════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════

float M5DialSonos::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

void M5DialSonos::setup() {
  ESP_LOGI(TAG, "Initialising M5Dial display...");

  // Init M5Dial — enable display, disable encoder (ESPHome owns GPIOs)
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* enableDisplay */, false /* enableEncoder */);

  M5Dial.Display.setRotation(0);
  M5Dial.Display.fillScreen(COL_BG);
  M5Dial.Display.setBrightness(BRIGHTNESS_FULL);

  last_interaction_ = millis();
  display_dirty_ = true;

  ESP_LOGI(TAG, "M5Dial display ready");
}

void M5DialSonos::loop() {
  M5Dial.update();

  // Screen dim timeout
  uint32_t now = millis();
  if (!screen_dimmed_ && !is_playing_ &&
      (now - last_interaction_) > screen_off_time_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_DIM);
    screen_dimmed_ = true;
  }

  // Redraw if needed
  if (display_dirty_) {
    display_dirty_ = false;
    refresh_display();
  }
}

// ═══════════════════════════════════════════════════════════════════════
// State setters
// ═══════════════════════════════════════════════════════════════════════

void M5DialSonos::set_playlist_json(const std::string &json) {
  playlist_names_.clear();

  std::string raw = json;

  // Trim whitespace
  size_t s = raw.find_first_not_of(" \t\r\n");
  size_t e = raw.find_last_not_of(" \t\r\n");
  if (s == std::string::npos) {
    raw.clear();
  } else {
    raw = raw.substr(s, e - s + 1);
  }

  if (raw.empty() || raw.front() != '[') {
    ESP_LOGW(TAG, "source_list is not a JSON array: %s", raw.c_str());
    if (playlist_index_ >= (int)playlist_names_.size()) playlist_index_ = 0;
    display_dirty_ = true;
    return;
  }

  // Strip outer [ ]
  raw = raw.substr(1, raw.size() > 2 ? raw.size() - 2 : 0);

  bool in_quote = false;
  bool escape_next = false;
  std::string token;

  for (char c : raw) {
    if (escape_next) {
      token += c;
      escape_next = false;
      continue;
    }
    if (c == '\\') { escape_next = true; continue; }
    if (c == '"') {
      if (!in_quote) {
        in_quote = true;
        token.clear();
      } else {
        if (!token.empty()) playlist_names_.push_back(token);
        in_quote = false;
      }
      continue;
    }
    if (in_quote) token += c;
  }

  // Also handle unquoted items (e.g. HA sometimes sends without quotes)
  // Already handled by the quote parser above

  if (playlist_index_ >= (int)playlist_names_.size()) {
    playlist_index_ = 0;
  }

  display_dirty_ = true;
  ESP_LOGI(TAG, "Parsed %d playlists", (int)playlist_names_.size());
}

void M5DialSonos::set_media_title(const std::string &title) {
  if (media_title_ != title) {
    media_title_ = title;
    if (mode_ == MODE_NOW_PLAYING) display_dirty_ = true;
  }
}

void M5DialSonos::set_media_artist(const std::string &artist) {
  if (media_artist_ != artist) {
    media_artist_ = artist;
    if (mode_ == MODE_NOW_PLAYING) display_dirty_ = true;
  }
}

void M5DialSonos::set_volume_level(float level) {
  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  volume_ = level;
  if (mode_ == MODE_VOLUME) display_dirty_ = true;
}

void M5DialSonos::set_player_state(const std::string &state) {
  bool was_playing = is_playing_;
  is_playing_ = (state == "playing");

  if (is_playing_ && !was_playing) {
    // Playback started: switch to Now Playing, wake screen
    mode_ = MODE_NOW_PLAYING;
    wake_screen();
  }

  display_dirty_ = true;
}

// ═══════════════════════════════════════════════════════════════════════
// State getters
// ═══════════════════════════════════════════════════════════════════════

std::string M5DialSonos::get_current_playlist_name() const {
  if (playlist_names_.empty()) return "";
  return playlist_names_[playlist_index_];
}

// ═══════════════════════════════════════════════════════════════════════
// Input handlers
// ═══════════════════════════════════════════════════════════════════════

void M5DialSonos::on_rotary_cw() {
  wake_screen();
  switch (mode_) {
    case MODE_PLAYLIST:
      if (!playlist_names_.empty()) {
        playlist_index_ = (playlist_index_ + 1) % playlist_names_.size();
        display_dirty_ = true;
      }
      break;
    case MODE_NOW_PLAYING:
      // Track skip handled in YAML
      break;
    case MODE_VOLUME: {
      float step = volume_step_ / 100.0f;
      volume_ += step;
      if (volume_ > 1.0f) volume_ = 1.0f;
      display_dirty_ = true;
      break;
    }
    default:
      break;
  }
}

void M5DialSonos::on_rotary_ccw() {
  wake_screen();
  switch (mode_) {
    case MODE_PLAYLIST:
      if (!playlist_names_.empty()) {
        playlist_index_ = (playlist_index_ - 1 + playlist_names_.size()) %
                          playlist_names_.size();
        display_dirty_ = true;
      }
      break;
    case MODE_NOW_PLAYING:
      // Track skip handled in YAML
      break;
    case MODE_VOLUME: {
      float step = volume_step_ / 100.0f;
      volume_ -= step;
      if (volume_ < 0.0f) volume_ = 0.0f;
      display_dirty_ = true;
      break;
    }
    default:
      break;
  }
}

void M5DialSonos::on_short_press() {
  wake_screen();
  // Action dispatched in YAML based on get_mode()
}

void M5DialSonos::on_long_press() {
  wake_screen();
  mode_ = static_cast<Mode>((mode_ + 1) % MODE_COUNT);
  display_dirty_ = true;
  // Beep handled in YAML
}

void M5DialSonos::wake_screen() {
  last_interaction_ = millis();
  if (screen_dimmed_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_FULL);
    screen_dimmed_ = false;
  }
}

// ═══════════════════════════════════════════════════════════════════════
// Drawing
// ═══════════════════════════════════════════════════════════════════════

void M5DialSonos::refresh_display() {
  switch (mode_) {
    case MODE_PLAYLIST:
      draw_page_playlist();
      break;
    case MODE_NOW_PLAYING:
      draw_page_now_playing();
      break;
    case MODE_VOLUME:
      draw_page_volume();
      break;
    default:
      break;
  }
}

void M5DialSonos::draw_page_playlist() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // Music note icon (orange circle with note symbol)
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setTextDatum(middle_center);
  dsp.setFont(&fonts::FreeSansBold18pt7b);
  dsp.drawString("~", CENTER_X, 32);  // decorative header

  if (playlist_names_.empty()) {
    dsp.setFont(&fonts::FreeSansBold12pt7b);
    dsp.setTextColor(COL_WHITE, COL_BG);
    dsp.drawString("No favourites", CENTER_X, CENTER_Y);
  } else {
    int n = playlist_names_.size();
    int prev_idx = (playlist_index_ - 1 + n) % n;
    int next_idx = (playlist_index_ + 1) % n;

    // Previous (dim, smaller)
    draw_clipped_string(CENTER_X, CENTER_Y - 44,
                        playlist_names_[prev_idx], 160,
                        COL_GREY_55, &fonts::FreeSansBold9pt7b);

    // Current (bright, larger)
    draw_clipped_string(CENTER_X, CENTER_Y,
                        playlist_names_[playlist_index_], 200,
                        COL_WHITE, &fonts::FreeSansBold12pt7b);

    // Next (dim, smaller)
    draw_clipped_string(CENTER_X, CENTER_Y + 44,
                        playlist_names_[next_idx], 160,
                        COL_GREY_55, &fonts::FreeSansBold9pt7b);
  }

  draw_mode_dots(MODE_PLAYLIST);
}

void M5DialSonos::draw_page_now_playing() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // Play/pause icon at top
  dsp.setTextDatum(middle_center);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setFont(&fonts::FreeSansBold12pt7b);
  if (is_playing_) {
    // Pause icon: two vertical bars
    dsp.fillRect(CENTER_X - 10, 18, 6, 20, COL_ORANGE);
    dsp.fillRect(CENTER_X + 4, 18, 6, 20, COL_ORANGE);
  } else {
    // Play icon: triangle
    dsp.fillTriangle(CENTER_X - 8, 18, CENTER_X - 8, 38,
                     CENTER_X + 10, 28, COL_ORANGE);
  }

  // Track title (centered)
  draw_clipped_string(CENTER_X, CENTER_Y - 10,
                      media_title_, 200,
                      COL_WHITE, &fonts::FreeSansBold12pt7b);

  // Artist (below title, grey)
  draw_clipped_string(CENTER_X, CENTER_Y + 24,
                      media_artist_, 200,
                      COL_GREY_CC, &fonts::FreeSansBold9pt7b);

  draw_mode_dots(MODE_NOW_PLAYING);
}

void M5DialSonos::draw_page_volume() {
  auto &dsp = M5Dial.Display;
  dsp.fillScreen(COL_BG);

  // "Volume" header
  dsp.setTextDatum(middle_center);
  dsp.setTextColor(COL_ORANGE, COL_BG);
  dsp.setFont(&fonts::FreeSansBold9pt7b);
  dsp.drawString("Volume", CENTER_X, 32);

  int pct = (int)(volume_ * 100.0f + 0.5f);

  // Background arc (270 degrees, gap at bottom)
  // Arc from 135° to 45° (going clockwise = 270°)
  dsp.fillArc(CENTER_X, CENTER_Y, 105, 97, 135, 405, COL_GREY_33);

  // Foreground arc (filled portion)
  if (pct > 0) {
    float end_angle = 135.0f + 270.0f * pct / 100.0f;
    dsp.fillArc(CENTER_X, CENTER_Y, 105, 97, 135, (int)end_angle, COL_ORANGE);
  }

  // Percentage text in centre
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  dsp.setTextColor(COL_WHITE, COL_BG);
  dsp.setFont(&fonts::FreeSansBold18pt7b);
  dsp.drawString(buf, CENTER_X, CENTER_Y);

  draw_mode_dots(MODE_VOLUME);
}

void M5DialSonos::draw_mode_dots(uint8_t active) {
  auto &dsp = M5Dial.Display;

  // Three dots near bottom-right, inside the circular display
  // Position them roughly at (175, 210), (191, 210), (207, 210)
  const int dot_y = 210;
  const int dot_x_start = 175;
  const int dot_spacing = 16;
  const int dot_r = 4;

  for (int i = 0; i < 3; i++) {
    int x = dot_x_start + i * dot_spacing;
    uint16_t color = (i == active) ? COL_ORANGE : COL_GREY_44;
    dsp.fillCircle(x, dot_y, dot_r, color);
  }
}

void M5DialSonos::draw_clipped_string(int32_t x, int32_t y,
                                       const std::string &text,
                                       int max_width, uint32_t color,
                                       const void *font) {
  auto &dsp = M5Dial.Display;
  dsp.setFont(static_cast<const lgfx::GFXfont *>(font));
  dsp.setTextColor(color, COL_BG);
  dsp.setTextDatum(middle_center);

  // Check if text fits
  int tw = dsp.textWidth(text.c_str());
  if (tw <= max_width) {
    dsp.drawString(text.c_str(), x, y);
  } else {
    // Truncate with "..."
    std::string truncated = text;
    int ellipsis_w = dsp.textWidth("...");
    while (truncated.size() > 1 &&
           dsp.textWidth(truncated.c_str()) + ellipsis_w > max_width) {
      truncated.pop_back();
    }
    truncated += "...";
    dsp.drawString(truncated.c_str(), x, y);
  }
}

}  // namespace m5dial_sonos
}  // namespace esphome
