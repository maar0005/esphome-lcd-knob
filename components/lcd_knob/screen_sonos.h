#pragma once

#include "screen.h"
#include <string>
#include <vector>
#include <cstdint>

namespace esphome {
namespace lcd_knob {

// ── Shared Sonos playback state ───────────────────────────────────────────────
// Owned by LcdKnob. All three Sonos screens hold a pointer to this.
struct SonosState {
  std::string entity;
  std::string ha_url;              // e.g. "http://homeassistant.local:8123"

  std::vector<std::string> playlist_names;
  int         playlist_index{0};
  float       volume{0.0f};
  bool        is_playing{false};
  std::string media_title{"—"};
  std::string media_artist;

  // Album art — filled by LcdKnob::do_fetch_album_art_()
  std::string          album_art_url;
  std::vector<uint8_t> album_art_data;
  bool                 album_art_valid{false};
};

// ── Playlist browser ──────────────────────────────────────────────────────────
class SonosPlaylistScreen : public Screen {
 public:
  explicit SonosPlaylistScreen(SonosState *state) : state_(state) {}

  void draw() override;
  void on_rotary_cw()  override;
  void on_rotary_ccw() override;

 private:
  SonosState *state_;
};

// ── Now playing ───────────────────────────────────────────────────────────────
// Full-screen album art with gradient overlay when art is available.
// Falls back to a seeded music-bar visualisation when art is not ready.
class SonosNowPlayingScreen : public Screen {
 public:
  explicit SonosNowPlayingScreen(SonosState *state) : state_(state) {}

  void draw() override;

 private:
  SonosState *state_;

  void draw_art_mode_();
  void draw_fallback_mode_();
};

// ── Volume control ────────────────────────────────────────────────────────────
class SonosVolumeScreen : public Screen {
 public:
  SonosVolumeScreen(SonosState *state, int volume_step)
      : state_(state), volume_step_(volume_step) {}

  void draw() override;
  void on_rotary_cw()  override;
  void on_rotary_ccw() override;

  int volume_step() const { return volume_step_; }

 private:
  SonosState *state_;
  int         volume_step_;
};

}  // namespace lcd_knob
}  // namespace esphome
