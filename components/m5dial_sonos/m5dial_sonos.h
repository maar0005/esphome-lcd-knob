#pragma once

#include "esphome/core/component.h"
#include <string>
#include <vector>

namespace esphome {
namespace m5dial_sonos {

// UI modes
enum Mode : uint8_t {
  MODE_PLAYLIST = 0,
  MODE_NOW_PLAYING = 1,
  MODE_VOLUME = 2,
  MODE_COUNT = 3,
};

class M5DialSonos : public Component {
 public:
  // ── ESPHome lifecycle ──────────────────────────────────────────────
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;

  // ── Config setters (called by generated code from __init__.py) ─────
  void set_sonos_entity(const std::string &entity) { sonos_entity_ = entity; }
  void set_screen_off_time(uint32_t ms) { screen_off_time_ = ms; }
  void set_long_press_duration(uint32_t ms) { long_press_duration_ = ms; }
  void set_volume_step(int step) { volume_step_ = step; }

  // ── State setters (called by YAML text_sensor on_value lambdas) ────
  void set_playlist_json(const std::string &json);
  void set_media_title(const std::string &title);
  void set_media_artist(const std::string &artist);
  void set_volume_level(float level);
  void set_player_state(const std::string &state);

  // ── State getters (read by YAML lambdas for HA service calls) ──────
  int get_mode() const { return mode_; }
  int get_playlist_count() const { return playlist_names_.size(); }
  std::string get_current_playlist_name() const;
  float get_volume() const { return volume_; }
  bool is_playing() const { return is_playing_; }

  // ── Input handlers (called by YAML rotary/button lambdas) ──────────
  void on_rotary_cw();
  void on_rotary_ccw();
  void on_short_press();
  void on_long_press();

  // ── Display control ────────────────────────────────────────────────
  void wake_screen();

 protected:
  // ── Drawing methods ────────────────────────────────────────────────
  void refresh_display();
  void draw_page_playlist();
  void draw_page_now_playing();
  void draw_page_volume();
  void draw_mode_dots(uint8_t active);
  void draw_clipped_string(int32_t x, int32_t y, const std::string &text,
                           int max_width, uint32_t color, const void *font);

  // ── Configuration ──────────────────────────────────────────────────
  std::string sonos_entity_;
  uint32_t screen_off_time_{30000};
  uint32_t long_press_duration_{800};
  int volume_step_{2};

  // ── State ──────────────────────────────────────────────────────────
  Mode mode_{MODE_PLAYLIST};
  std::vector<std::string> playlist_names_;
  int playlist_index_{0};
  float volume_{0.0f};
  bool is_playing_{false};
  std::string media_title_{"—"};
  std::string media_artist_;

  // ── Display state ──────────────────────────────────────────────────
  bool display_dirty_{true};
  bool screen_dimmed_{false};
  uint32_t last_interaction_{0};
};

}  // namespace m5dial_sonos
}  // namespace esphome
