#pragma once

#include "esphome/core/component.h"
#include "screen.h"
#include "screen_menu.h"
#include "screen_sonos.h"
#include "screen_meater.h"
#include "screen_timer.h"
#include "screen_alarm.h"
#include "screen_countup.h"
#include "screen_light.h"
#include <vector>
#include <string>

namespace esphome {
namespace lcd_knob {

// ── Screen group ──────────────────────────────────────────────────────────────
struct ScreenGroup {
  std::string          name;
  std::vector<Screen*> pages;
  size_t               current_page{0};
};

// ── Config queue entries (pre-setup) ─────────────────────────────────────────
enum class ScreenKind : uint8_t { SONOS, MEATER, TIMER, TIMER2, ALARM, ALARM2, COUNTUP, LIGHT };

struct ScreenConfig {
  ScreenKind  kind;
  std::string sonos_entity;
  std::string sonos_ha_url;          // base URL for fetching album art
  int         sonos_volume_step{2};
  std::string meater_entity_temp;
  std::string meater_entity_target;
  std::string meater_entity_ambient;
  uint32_t    timer_default_s{300};
  std::string light_entity;
  std::string light_name;
};

// ══════════════════════════════════════════════════════════════════════════════
class LcdKnob : public Component {
 public:
  // ── ESPHome lifecycle ──────────────────────────────────────────────────────
  void  setup() override;
  void  loop()  override;
  float get_setup_priority() const override;

  // ── Global config ──────────────────────────────────────────────────────────
  void set_screen_off_time    (uint32_t ms) { screen_off_time_    = ms; }
  void set_long_press_duration(uint32_t ms) { long_press_duration_ = ms; }

  // ── Screen declarations — called in order from generated code ─────────────
  void configure_sonos  (const std::string &entity, int volume_step,
                         const std::string &ha_url = "");
  void configure_meater (const std::string &entity_temp,
                         const std::string &entity_target,
                         const std::string &entity_ambient);
  void configure_timer  (uint32_t default_s);
  void configure_timer2 (uint32_t default_s);
  void configure_alarm  ();
  void configure_alarm2 ();
  void configure_countup();
  void configure_light(const std::string &entity, const std::string &name);

  // ── Menu control ──────────────────────────────────────────────────────────
  void open_menu();
  void close_menu();
  void toggle_menu();
  bool is_in_menu() const { return in_menu_; }

  // ── Sonos state setters ────────────────────────────────────────────────────
  void set_playlist_json (const std::string &json);
  void set_media_title   (const std::string &title);
  void set_media_artist  (const std::string &artist);
  void set_volume_level  (float level);
  void set_player_state  (const std::string &state);
  void set_album_art_url (const std::string &url);   // triggers async HTTP fetch

  // ── Sonos getters ──────────────────────────────────────────────────────────
  float       get_volume()                const;
  int         get_playlist_count()        const;
  std::string get_current_playlist_name() const;

  // ── Screen-type queries ────────────────────────────────────────────────────
  bool is_sonos_playlist()    const;
  bool is_sonos_now_playing() const;
  bool is_sonos_volume()      const;
  bool is_meater()            const;

  // ── Meater state setters ───────────────────────────────────────────────────
  void set_meater_temperature(float t);
  void set_meater_target     (float t);
  void set_meater_ambient    (float t);

  // ── Timer 1 API ────────────────────────────────────────────────────────────
  void     set_timer_duration(uint32_t s);
  void     start_timer();
  void     stop_timer();
  bool     is_timer_running()    const;
  uint32_t get_timer_remaining() const;
  bool     is_timer_fired()      const;
  void     clear_timer_fired();

  // ── Timer 2 API ────────────────────────────────────────────────────────────
  void     set_timer2_duration(uint32_t s);
  void     start_timer2();
  void     stop_timer2();
  bool     is_timer2_running()    const;
  uint32_t get_timer2_remaining() const;
  bool     is_timer2_fired()      const;
  void     clear_timer2_fired();

  // ── Alarm 1 API ────────────────────────────────────────────────────────────
  void    set_alarm_hour(uint8_t h);
  void    set_alarm_minute(uint8_t m);
  void    arm_alarm();
  void    disarm_alarm();
  bool    is_alarm_armed()   const;
  bool    is_alarm_fired()   const;
  void    clear_alarm_fired();
  uint8_t get_alarm_hour()   const;
  uint8_t get_alarm_minute() const;

  // ── Alarm 2 API ────────────────────────────────────────────────────────────
  void    set_alarm2_hour(uint8_t h);
  void    set_alarm2_minute(uint8_t m);
  void    arm_alarm2();
  void    disarm_alarm2();
  bool    is_alarm2_armed()   const;
  bool    is_alarm2_fired()   const;
  void    clear_alarm2_fired();
  uint8_t get_alarm2_hour()   const;
  uint8_t get_alarm2_minute() const;

  // ── Light API ──────────────────────────────────────────────────────────────
  void        on_light_on_state  (const std::string &entity, bool is_on);
  void        on_light_brightness(const std::string &entity, uint8_t pct);
  bool        is_light_screen()      const;
  bool        is_light_pending()     const;
  void        clear_light_pending();
  std::string get_light_entity()     const;
  bool        get_light_on()         const;
  uint8_t     get_light_brightness() const;

  // ── Count-up API ───────────────────────────────────────────────────────────
  void     start_countup();
  void     reset_countup();
  bool     is_countup_running()  const;
  uint32_t get_countup_elapsed() const;

  // ── Alarm time check ───────────────────────────────────────────────────────
  void check_alarms(uint8_t hour, uint8_t minute);

  // ── Input handlers ────────────────────────────────────────────────────────
  void on_rotary_cw();
  void on_rotary_ccw();
  void on_short_press();
  void on_long_press();
  void wake_screen();

 private:
  std::vector<ScreenConfig> screen_configs_;

  std::vector<ScreenGroup> groups_;
  size_t current_group_{0};
  bool   in_menu_{false};

  MenuScreen            *menu_screen_      {nullptr};
  SonosState             sonos_state_;
  SonosPlaylistScreen   *sonos_playlist_   {nullptr};
  SonosNowPlayingScreen *sonos_now_playing_{nullptr};
  SonosVolumeScreen     *sonos_volume_     {nullptr};
  MeaterScreen          *meater_           {nullptr};

  TimerState   timer1_state_;
  TimerState   timer2_state_;
  TimerScreen *timer1_screen_{nullptr};
  TimerScreen *timer2_screen_{nullptr};

  AlarmState   alarm1_state_;
  AlarmState   alarm2_state_;
  AlarmScreen *alarm1_screen_{nullptr};
  AlarmScreen *alarm2_screen_{nullptr};

  CountUpState   countup_state_;
  CountUpScreen *countup_screen_{nullptr};

  std::vector<LightState*>  light_states_;
  std::vector<LightScreen*> light_screens_;

  bool any_active() const {
    return sonos_state_.is_playing
        || timer1_state_.running
        || timer2_state_.running
        || countup_state_.running;
  }

  // ── Display / timeout ─────────────────────────────────────────────────────
  uint32_t screen_off_time_    {30000};
  uint32_t long_press_duration_{800};
  uint32_t last_interaction_   {0};
  bool     screen_dimmed_      {false};

  // ── Album art screensaver ──────────────────────────────────────────────────
  // When music is playing and idle for ART_RETURN_MS, snap back to Now Playing.
  static constexpr uint32_t ART_RETURN_MS = 10000;  // 10 s

  // ── Album art HTTP fetch ───────────────────────────────────────────────────
  bool        art_fetch_pending_{false};
  std::string art_url_pending_;
  void        do_fetch_album_art_();

  // ── Touch double-tap detection ────────────────────────────────────────────
  uint32_t last_tap_ms_{0};
  static constexpr uint32_t DOUBLE_TAP_MS = 400;

  static constexpr uint8_t BRIGHTNESS_FULL = 100;
  static constexpr uint8_t BRIGHTNESS_DIM  = 25;

  Screen *current_screen()     const;
  void    next_page_in_group();
  void    navigate_to_now_playing_();
  void    draw_mode_dots();
};

}  // namespace lcd_knob
}  // namespace esphome
