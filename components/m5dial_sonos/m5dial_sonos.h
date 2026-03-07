#pragma once

#include "esphome/core/component.h"
#include "screen.h"
#include "screen_menu.h"
#include "screen_sonos.h"
#include "screen_meater.h"
#include <vector>
#include <string>

namespace esphome {
namespace m5dial_sonos {

// ── Screen group ──────────────────────────────────────────────────────────────
// A named collection of pages (screens).  Long press cycles within the group.
// The menu lists one entry per group.
struct ScreenGroup {
  std::string          name;
  std::vector<Screen*> pages;
  size_t               current_page{0};
};

// ── Config queue entries (pre-setup) ─────────────────────────────────────────
enum class ScreenKind : uint8_t { SONOS, MEATER };

struct ScreenConfig {
  ScreenKind  kind;
  std::string sonos_entity;
  int         sonos_volume_step{2};
  std::string meater_entity_temp;
  std::string meater_entity_target;
  std::string meater_entity_ambient;
};

// ══════════════════════════════════════════════════════════════════════════════
class M5DialSonos : public Component {
 public:
  // ── ESPHome lifecycle ──────────────────────────────────────────────────────
  void  setup() override;
  void  loop()  override;
  float get_setup_priority() const override;

  // ── Global config ──────────────────────────────────────────────────────────
  void set_screen_off_time    (uint32_t ms) { screen_off_time_    = ms; }
  void set_long_press_duration(uint32_t ms) { long_press_duration_ = ms; }

  // ── Screen declarations — called in order from generated code ─────────────
  void configure_sonos (const std::string &entity, int volume_step);
  void configure_meater(const std::string &entity_temp,
                        const std::string &entity_target,
                        const std::string &entity_ambient);

  // ── Menu control (also triggered from touch double-tap in loop()) ─────────
  void open_menu();
  void close_menu();
  void toggle_menu();
  bool is_in_menu() const { return in_menu_; }

  // ── Sonos state setters — called from YAML text_sensor lambdas ────────────
  void set_playlist_json (const std::string &json);
  void set_media_title   (const std::string &title);
  void set_media_artist  (const std::string &artist);
  void set_volume_level  (float level);
  void set_player_state  (const std::string &state);

  // ── Sonos getters — used by YAML action scripts ───────────────────────────
  float       get_volume()                const;
  int         get_playlist_count()        const;
  std::string get_current_playlist_name() const;

  // ── Screen-type queries — readable dispatching in YAML lambdas ────────────
  bool is_sonos_playlist()    const;
  bool is_sonos_now_playing() const;
  bool is_sonos_volume()      const;
  bool is_meater()            const;

  // ── Meater state setters — called from YAML sensor lambdas ───────────────
  void set_meater_temperature(float t);
  void set_meater_target     (float t);
  void set_meater_ambient    (float t);

  // ── Input handlers — called from YAML rotary / button lambdas ────────────
  void on_rotary_cw();
  void on_rotary_ccw();
  void on_short_press();
  void on_long_press();
  void wake_screen();

 private:
  // ── Config queue (populated before setup()) ────────────────────────────────
  std::vector<ScreenConfig> screen_configs_;

  // ── Runtime ────────────────────────────────────────────────────────────────
  std::vector<ScreenGroup> groups_;
  size_t current_group_{0};
  bool   in_menu_{false};

  MenuScreen            *menu_screen_    {nullptr};

  // Typed pointers for state forwarding (null until configured)
  SonosState             sonos_state_;
  SonosPlaylistScreen   *sonos_playlist_  {nullptr};
  SonosNowPlayingScreen *sonos_now_playing_{nullptr};
  SonosVolumeScreen     *sonos_volume_    {nullptr};
  MeaterScreen          *meater_          {nullptr};

  // ── Display / timeout ─────────────────────────────────────────────────────
  uint32_t screen_off_time_    {30000};
  uint32_t long_press_duration_{800};
  uint32_t last_interaction_   {0};
  bool     screen_dimmed_      {false};

  // ── Touch double-tap detection ────────────────────────────────────────────
  uint32_t last_tap_ms_{0};
  static constexpr uint32_t DOUBLE_TAP_MS = 400;

  static constexpr uint8_t BRIGHTNESS_FULL = 100;
  static constexpr uint8_t BRIGHTNESS_DIM  = 25;

  Screen *current_screen() const;
  void    next_page_in_group();
  void    draw_mode_dots();
};

}  // namespace m5dial_sonos
}  // namespace esphome
