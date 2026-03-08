#include "lcd_knob.h"
#include "esphome/core/log.h"
#include <M5Dial.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace esphome {
namespace lcd_knob {

static const char *const TAG = "lcd_knob";

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

float LcdKnob::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

void LcdKnob::setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* encoder */, false /* RFID */);
  M5Dial.Display.setRotation(0);
  M5Dial.Display.fillScreen(COL_BG);
  M5Dial.Display.setBrightness(BRIGHTNESS_FULL);
  last_interaction_ = millis();

  // ── Build groups in declaration order ─────────────────────────────────────
  std::vector<std::string> group_names;
  ScreenGroup lights_group;  lights_group.name = "Lights";
  ScreenGroup alarms_group;  alarms_group.name = "Alarms";

  for (auto &sc : screen_configs_) {
    switch (sc.kind) {
      case ScreenKind::SONOS: {
        ScreenGroup g;
        g.name                = "Sonos";
        sonos_state_.entity   = sc.sonos_entity;
        sonos_state_.ha_url   = sc.sonos_ha_url;
        sonos_playlist_    = new SonosPlaylistScreen(&sonos_state_);
        sonos_now_playing_ = new SonosNowPlayingScreen(&sonos_state_);
        sonos_volume_      = new SonosVolumeScreen(&sonos_state_, sc.sonos_volume_step);
        g.pages = {sonos_playlist_, sonos_now_playing_, sonos_volume_};
        groups_.push_back(std::move(g));
        group_names.push_back("Sonos");
        break;
      }
      case ScreenKind::MEATER: {
        ScreenGroup g;
        g.name  = "Meater";
        meater_ = new MeaterScreen();
        if (!sc.meater_entity_temp.empty())    meater_->set_entity_temperature(sc.meater_entity_temp);
        if (!sc.meater_entity_target.empty())  meater_->set_entity_target(sc.meater_entity_target);
        if (!sc.meater_entity_ambient.empty()) meater_->set_entity_ambient(sc.meater_entity_ambient);
        g.pages = {meater_};
        groups_.push_back(std::move(g));
        group_names.push_back("Meater");
        break;
      }
      case ScreenKind::TIMER: {
        timer1_state_.duration_s  = sc.timer_default_s;
        timer1_state_.remaining_s = sc.timer_default_s;
        timer1_screen_ = new TimerScreen("TIMER 1", &timer1_state_);
        alarms_group.pages.push_back(timer1_screen_);
        break;
      }
      case ScreenKind::TIMER2: {
        timer2_state_.duration_s  = sc.timer_default_s;
        timer2_state_.remaining_s = sc.timer_default_s;
        timer2_screen_ = new TimerScreen("TIMER 2", &timer2_state_);
        alarms_group.pages.push_back(timer2_screen_);
        break;
      }
      case ScreenKind::ALARM: {
        alarm1_screen_ = new AlarmScreen("ALARM 1", &alarm1_state_);
        alarms_group.pages.push_back(alarm1_screen_);
        break;
      }
      case ScreenKind::ALARM2: {
        alarm2_screen_ = new AlarmScreen("ALARM 2", &alarm2_state_);
        alarms_group.pages.push_back(alarm2_screen_);
        break;
      }
      case ScreenKind::COUNTUP: {
        countup_screen_ = new CountUpScreen(&countup_state_);
        alarms_group.pages.push_back(countup_screen_);
        break;
      }
      case ScreenKind::LIGHT: {
        auto *ls   = new LightState();
        ls->entity = sc.light_entity;
        ls->name   = sc.light_name;
        light_states_.push_back(ls);
        auto *lsc = new LightScreen(ls);
        light_screens_.push_back(lsc);
        lights_group.pages.push_back(lsc);
        break;
      }
    }
  }

  if (!lights_group.pages.empty()) {
    groups_.push_back(std::move(lights_group));
    group_names.push_back("Lights");
  }
  if (!alarms_group.pages.empty()) {
    groups_.push_back(std::move(alarms_group));
    group_names.push_back("Alarms");
  }

  menu_screen_ = new MenuScreen();
  menu_screen_->set_groups(group_names);

  ESP_LOGI(TAG, "M5Dial ready — %d group(s) registered", (int)groups_.size());
}

void LcdKnob::loop() {
  M5Dial.update();

  uint32_t now = millis();

  // ── Album art screensaver: auto-return to Now Playing when idle + playing ──
  if (sonos_state_.is_playing && sonos_now_playing_ &&
      current_screen() != sonos_now_playing_ && !in_menu_ &&
      (now - last_interaction_) > ART_RETURN_MS) {
    navigate_to_now_playing_();
  }

  // ── Pending album art fetch (blocks ~0.5–2 s; done only on track change) ──
  if (art_fetch_pending_) {
    art_fetch_pending_ = false;
    do_fetch_album_art_();
  }

  // ── Tick running timers and count-up ──────────────────────────────────────
  if (timer1_screen_)  timer1_screen_->tick(now);
  if (timer2_screen_)  timer2_screen_->tick(now);
  if (countup_screen_) countup_screen_->tick(now);

  // ── Screen dim (suppressed while music is playing) ────────────────────────
  if (!screen_dimmed_ && !any_active() && screen_off_time_ > 0 &&
      (now - last_interaction_) > screen_off_time_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_DIM);
    screen_dimmed_ = true;
  }

  // ── Touch double-tap → toggle menu ───────────────────────────────────────
  auto touch = M5Dial.Touch.getDetail();
  if (touch.wasPressed()) {
    if (last_tap_ms_ != 0 && (now - last_tap_ms_) < DOUBLE_TAP_MS) {
      toggle_menu();
      last_tap_ms_ = 0;
    } else {
      last_tap_ms_ = now;
    }
  }

  // ── Redraw if dirty ───────────────────────────────────────────────────────
  Screen *s = current_screen();
  if (s && s->is_dirty()) {
    s->clear_dirty();
    s->draw();
    draw_mode_dots();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════

Screen *LcdKnob::current_screen() const {
  if (in_menu_) return menu_screen_;
  if (groups_.empty()) return nullptr;
  const auto &g = groups_[current_group_];
  if (g.pages.empty()) return nullptr;
  return g.pages[g.current_page];
}

void LcdKnob::next_page_in_group() {
  if (groups_.empty()) return;
  auto &g = groups_[current_group_];
  if (g.pages.size() <= 1) return;
  g.current_page = (g.current_page + 1) % g.pages.size();
  g.pages[g.current_page]->mark_dirty();
}

void LcdKnob::navigate_to_now_playing_() {
  if (!sonos_now_playing_) return;
  for (size_t gi = 0; gi < groups_.size(); ++gi) {
    auto &g = groups_[gi];
    for (size_t pi = 0; pi < g.pages.size(); ++pi) {
      if (g.pages[pi] == sonos_now_playing_) {
        current_group_ = gi;
        g.current_page = pi;
        in_menu_       = false;
        sonos_now_playing_->mark_dirty();
        return;
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Screen declarations (pre-setup, called from generated code)
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::configure_sonos(const std::string &entity, int volume_step,
                               const std::string &ha_url) {
  ScreenConfig sc;
  sc.kind              = ScreenKind::SONOS;
  sc.sonos_entity      = entity;
  sc.sonos_volume_step = volume_step;
  sc.sonos_ha_url      = ha_url;
  screen_configs_.push_back(sc);
}

void LcdKnob::configure_meater(const std::string &t,
                                const std::string &tgt,
                                const std::string &amb) {
  ScreenConfig sc;
  sc.kind                  = ScreenKind::MEATER;
  sc.meater_entity_temp    = t;
  sc.meater_entity_target  = tgt;
  sc.meater_entity_ambient = amb;
  screen_configs_.push_back(sc);
}

void LcdKnob::configure_timer (uint32_t d) { ScreenConfig sc; sc.kind = ScreenKind::TIMER;   sc.timer_default_s = d; screen_configs_.push_back(sc); }
void LcdKnob::configure_timer2(uint32_t d) { ScreenConfig sc; sc.kind = ScreenKind::TIMER2;  sc.timer_default_s = d; screen_configs_.push_back(sc); }
void LcdKnob::configure_alarm ()           { ScreenConfig sc; sc.kind = ScreenKind::ALARM;   screen_configs_.push_back(sc); }
void LcdKnob::configure_alarm2()           { ScreenConfig sc; sc.kind = ScreenKind::ALARM2;  screen_configs_.push_back(sc); }
void LcdKnob::configure_countup()          { ScreenConfig sc; sc.kind = ScreenKind::COUNTUP; screen_configs_.push_back(sc); }

void LcdKnob::configure_light(const std::string &entity, const std::string &name) {
  ScreenConfig sc;
  sc.kind         = ScreenKind::LIGHT;
  sc.light_entity = entity;
  sc.light_name   = name;
  screen_configs_.push_back(sc);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Menu control
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::open_menu() {
  if (in_menu_) return;
  in_menu_ = true;
  if (menu_screen_) {
    menu_screen_->set_selection(current_group_);
    menu_screen_->mark_dirty();
  }
  wake_screen();
  ESP_LOGD(TAG, "Menu opened");
}

void LcdKnob::close_menu() {
  if (!in_menu_) return;
  in_menu_ = false;
  Screen *s = current_screen();
  if (s) s->mark_dirty();
  ESP_LOGD(TAG, "Menu closed → group %d", (int)current_group_);
}

void LcdKnob::toggle_menu() {
  if (in_menu_) close_menu();
  else          open_menu();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Input
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::on_rotary_cw()  { wake_screen(); Screen *s = current_screen(); if (s) s->on_rotary_cw(); }
void LcdKnob::on_rotary_ccw() { wake_screen(); Screen *s = current_screen(); if (s) s->on_rotary_ccw(); }

void LcdKnob::on_short_press() {
  wake_screen();
  if (in_menu_) {
    if (menu_screen_) current_group_ = menu_screen_->get_selection();
    close_menu();
    return;
  }
  Screen *s = current_screen();
  if (s) s->on_short_press();
}

void LcdKnob::on_long_press() {
  wake_screen();
  if (in_menu_) { close_menu(); return; }
  Screen *s = current_screen();
  if (s && s->on_long_press()) return;
  next_page_in_group();
}

void LcdKnob::wake_screen() {
  last_interaction_ = millis();
  if (screen_dimmed_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_FULL);
    screen_dimmed_ = false;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sonos state setters
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_playlist_json(const std::string &json) {
  if (!sonos_playlist_) return;
  sonos_state_.playlist_names.clear();

  std::string raw = json;
  size_t s = raw.find_first_not_of(" \t\r\n");
  size_t e = raw.find_last_not_of(" \t\r\n");
  if (s == std::string::npos) { raw.clear(); }
  else raw = raw.substr(s, e - s + 1);

  if (raw.empty() || raw.front() != '[') {
    ESP_LOGW(TAG, "source_list is not a JSON array");
    sonos_playlist_->mark_dirty();
    return;
  }
  raw = raw.substr(1, raw.size() > 2 ? raw.size() - 2 : 0);

  bool in_quote = false, esc = false;
  std::string token;
  for (char c : raw) {
    if (esc)        { token += c; esc = false; continue; }
    if (c == '\\')  { esc = true; continue; }
    if (c == '"') {
      if (!in_quote) { in_quote = true; token.clear(); }
      else { if (!token.empty()) sonos_state_.playlist_names.push_back(token); in_quote = false; }
      continue;
    }
    if (in_quote) token += c;
  }

  if (sonos_state_.playlist_index >= (int)sonos_state_.playlist_names.size())
    sonos_state_.playlist_index = 0;

  sonos_playlist_->mark_dirty();
  ESP_LOGI(TAG, "Parsed %d playlists", (int)sonos_state_.playlist_names.size());
}

void LcdKnob::set_media_title(const std::string &title) {
  if (sonos_state_.media_title == title) return;
  sonos_state_.media_title = title;
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

void LcdKnob::set_media_artist(const std::string &artist) {
  if (sonos_state_.media_artist == artist) return;
  sonos_state_.media_artist = artist;
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

void LcdKnob::set_volume_level(float level) {
  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  sonos_state_.volume = level;
  if (sonos_volume_) sonos_volume_->mark_dirty();
}

void LcdKnob::set_player_state(const std::string &state) {
  bool was_playing = sonos_state_.is_playing;
  sonos_state_.is_playing = (state == "playing");

  // Jump to Now Playing the moment playback starts
  if (sonos_state_.is_playing && !was_playing) {
    navigate_to_now_playing_();
    wake_screen();
  }

  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

// ── Album art ─────────────────────────────────────────────────────────────────

void LcdKnob::set_album_art_url(const std::string &url) {
  if (url == sonos_state_.album_art_url) return;
  sonos_state_.album_art_url   = url;
  sonos_state_.album_art_valid = false;
  sonos_state_.album_art_data.clear();
  art_url_pending_   = url;
  art_fetch_pending_ = !url.empty();
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

void LcdKnob::do_fetch_album_art_() {
  std::string url = art_url_pending_;
  if (url.empty()) return;

  // Relative URL → prepend HA base URL
  if (url[0] == '/') {
    if (sonos_state_.ha_url.empty()) {
      ESP_LOGW(TAG, "Album art URL is relative but ha_url not configured");
      return;
    }
    url = sonos_state_.ha_url + url;
  }

  ESP_LOGI(TAG, "Fetching album art: %.80s", url.c_str());

  HTTPClient   http;
  WiFiClientSecure wcs;
  const bool   is_https = (url.rfind("https://", 0) == 0);

  if (is_https) {
    wcs.setInsecure();  // skip cert verification — art CDN, low risk
    http.begin(wcs, url.c_str());
  } else {
    http.begin(url.c_str());
  }

  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const int code = http.GET();

  if (code == HTTP_CODE_OK) {
    const int len = http.getSize();
    if (len > 0 && len <= 200000) {
      // Content-Length known — read in one shot
      sonos_state_.album_art_data.resize((size_t)len);
      int n = http.getStream().readBytes(
          reinterpret_cast<char *>(sonos_state_.album_art_data.data()), len);
      sonos_state_.album_art_valid = (n == len);
    } else {
      // Chunked transfer — accumulate until stream closes
      WiFiClient *stream = http.getStreamPtr();
      sonos_state_.album_art_data.clear();
      sonos_state_.album_art_data.reserve(65536);
      uint8_t  chunk[512];
      uint32_t deadline = millis() + 5000;
      while (millis() < deadline) {
        int avail = stream->available();
        if (avail > 0) {
          int n = stream->read(chunk, avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk));
          if (n > 0) {
            sonos_state_.album_art_data.insert(sonos_state_.album_art_data.end(),
                                               chunk, chunk + n);
            deadline = millis() + 1000;  // extend on progress
          }
          if (sonos_state_.album_art_data.size() > 200000) break;
        } else if (!stream->connected()) {
          break;
        } else {
          delay(2);
        }
      }
      sonos_state_.album_art_valid = !sonos_state_.album_art_data.empty();
    }
    ESP_LOGI(TAG, "Album art: %d bytes, valid=%d",
             (int)sonos_state_.album_art_data.size(),
             (int)sonos_state_.album_art_valid);
  } else {
    ESP_LOGW(TAG, "Album art HTTP %d for %.80s", code, url.c_str());
  }

  http.end();
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sonos getters
// ═══════════════════════════════════════════════════════════════════════════════

float       LcdKnob::get_volume()                const { return sonos_state_.volume; }
int         LcdKnob::get_playlist_count()        const { return (int)sonos_state_.playlist_names.size(); }
std::string LcdKnob::get_current_playlist_name() const {
  if (sonos_state_.playlist_names.empty()) return "";
  return sonos_state_.playlist_names[sonos_state_.playlist_index];
}

// ═══════════════════════════════════════════════════════════════════════════════
// Screen-type queries
// ═══════════════════════════════════════════════════════════════════════════════

bool LcdKnob::is_sonos_playlist()    const { return !in_menu_ && current_screen() == sonos_playlist_;    }
bool LcdKnob::is_sonos_now_playing() const { return !in_menu_ && current_screen() == sonos_now_playing_; }
bool LcdKnob::is_sonos_volume()      const { return !in_menu_ && current_screen() == sonos_volume_;      }
bool LcdKnob::is_meater()            const { return !in_menu_ && current_screen() == meater_;            }

// ═══════════════════════════════════════════════════════════════════════════════
// Meater state setters
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_meater_temperature(float t) { if (meater_) meater_->set_temperature(t); }
void LcdKnob::set_meater_target     (float t) { if (meater_) meater_->set_target(t);      }
void LcdKnob::set_meater_ambient    (float t) { if (meater_) meater_->set_ambient(t);     }

// ═══════════════════════════════════════════════════════════════════════════════
// Timer 1
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_timer_duration(uint32_t s) {
  timer1_state_.duration_s = s;
  if (!timer1_state_.running) {
    timer1_state_.remaining_s     = s;
    timer1_state_.elapsed_at_pause = 0;
    if (timer1_screen_) timer1_screen_->mark_dirty();
  }
}
void LcdKnob::start_timer() { if (timer1_screen_ && !timer1_state_.running) timer1_screen_->on_short_press(); }
void LcdKnob::stop_timer()  { if (timer1_screen_ &&  timer1_state_.running) timer1_screen_->on_short_press(); }
bool     LcdKnob::is_timer_running()    const { return timer1_state_.running;    }
uint32_t LcdKnob::get_timer_remaining() const { return timer1_state_.remaining_s; }
bool     LcdKnob::is_timer_fired()      const { return timer1_state_.fired;       }
void LcdKnob::clear_timer_fired() { timer1_state_.fired = false; if (timer1_screen_) timer1_screen_->mark_dirty(); }

// ═══════════════════════════════════════════════════════════════════════════════
// Timer 2
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_timer2_duration(uint32_t s) {
  timer2_state_.duration_s = s;
  if (!timer2_state_.running) {
    timer2_state_.remaining_s     = s;
    timer2_state_.elapsed_at_pause = 0;
    if (timer2_screen_) timer2_screen_->mark_dirty();
  }
}
void LcdKnob::start_timer2() { if (timer2_screen_ && !timer2_state_.running) timer2_screen_->on_short_press(); }
void LcdKnob::stop_timer2()  { if (timer2_screen_ &&  timer2_state_.running) timer2_screen_->on_short_press(); }
bool     LcdKnob::is_timer2_running()    const { return timer2_state_.running;    }
uint32_t LcdKnob::get_timer2_remaining() const { return timer2_state_.remaining_s; }
bool     LcdKnob::is_timer2_fired()      const { return timer2_state_.fired;       }
void LcdKnob::clear_timer2_fired() { timer2_state_.fired = false; if (timer2_screen_) timer2_screen_->mark_dirty(); }

// ═══════════════════════════════════════════════════════════════════════════════
// Alarm 1
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_alarm_hour  (uint8_t h) { alarm1_state_.hour   = h % 24; if (alarm1_screen_) alarm1_screen_->mark_dirty(); }
void LcdKnob::set_alarm_minute(uint8_t m) { alarm1_state_.minute = m % 60; if (alarm1_screen_) alarm1_screen_->mark_dirty(); }
void LcdKnob::arm_alarm()   { alarm1_state_.armed = true;  alarm1_state_.fired = false; if (alarm1_screen_) alarm1_screen_->mark_dirty(); }
void LcdKnob::disarm_alarm() { alarm1_state_.armed = false; alarm1_state_.fired = false; if (alarm1_screen_) alarm1_screen_->mark_dirty(); }
bool    LcdKnob::is_alarm_armed()   const { return alarm1_state_.armed;  }
bool    LcdKnob::is_alarm_fired()   const { return alarm1_state_.fired;  }
uint8_t LcdKnob::get_alarm_hour()   const { return alarm1_state_.hour;   }
uint8_t LcdKnob::get_alarm_minute() const { return alarm1_state_.minute; }
void LcdKnob::clear_alarm_fired() { alarm1_state_.fired = false; if (alarm1_screen_) alarm1_screen_->mark_dirty(); }

// ═══════════════════════════════════════════════════════════════════════════════
// Alarm 2
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::set_alarm2_hour  (uint8_t h) { alarm2_state_.hour   = h % 24; if (alarm2_screen_) alarm2_screen_->mark_dirty(); }
void LcdKnob::set_alarm2_minute(uint8_t m) { alarm2_state_.minute = m % 60; if (alarm2_screen_) alarm2_screen_->mark_dirty(); }
void LcdKnob::arm_alarm2()   { alarm2_state_.armed = true;  alarm2_state_.fired = false; if (alarm2_screen_) alarm2_screen_->mark_dirty(); }
void LcdKnob::disarm_alarm2() { alarm2_state_.armed = false; alarm2_state_.fired = false; if (alarm2_screen_) alarm2_screen_->mark_dirty(); }
bool    LcdKnob::is_alarm2_armed()   const { return alarm2_state_.armed;  }
bool    LcdKnob::is_alarm2_fired()   const { return alarm2_state_.fired;  }
uint8_t LcdKnob::get_alarm2_hour()   const { return alarm2_state_.hour;   }
uint8_t LcdKnob::get_alarm2_minute() const { return alarm2_state_.minute; }
void LcdKnob::clear_alarm2_fired() { alarm2_state_.fired = false; if (alarm2_screen_) alarm2_screen_->mark_dirty(); }

// ═══════════════════════════════════════════════════════════════════════════════
// Lights
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::on_light_on_state(const std::string &entity, bool is_on) {
  for (size_t i = 0; i < light_states_.size(); i++) {
    if (light_states_[i]->entity == entity) {
      light_states_[i]->is_on = is_on;
      light_screens_[i]->mark_dirty();
      return;
    }
  }
}

void LcdKnob::on_light_brightness(const std::string &entity, uint8_t pct) {
  for (size_t i = 0; i < light_states_.size(); i++) {
    if (light_states_[i]->entity == entity) {
      light_states_[i]->brightness = pct;
      light_screens_[i]->mark_dirty();
      return;
    }
  }
}

static LightScreen *active_light_screen(const std::vector<LightScreen *> &screens, Screen *cur) {
  for (auto *ls : screens) if (ls == cur) return ls;
  return nullptr;
}

bool        LcdKnob::is_light_screen()  const { return !in_menu_ && active_light_screen(light_screens_, current_screen()) != nullptr; }
bool        LcdKnob::is_light_pending() const { auto *l = active_light_screen(light_screens_, current_screen()); return l && l->light_state()->pending; }
void        LcdKnob::clear_light_pending()    { auto *l = active_light_screen(light_screens_, current_screen()); if (l) const_cast<LightState*>(l->light_state())->pending = false; }
std::string LcdKnob::get_light_entity() const { auto *l = active_light_screen(light_screens_, current_screen()); return l ? l->light_state()->entity : ""; }
bool        LcdKnob::get_light_on()     const { auto *l = active_light_screen(light_screens_, current_screen()); return l && l->light_state()->is_on; }
uint8_t     LcdKnob::get_light_brightness() const { auto *l = active_light_screen(light_screens_, current_screen()); return l ? l->light_state()->brightness : 0; }

// ═══════════════════════════════════════════════════════════════════════════════
// Count-up
// ═══════════════════════════════════════════════════════════════════════════════

void     LcdKnob::start_countup()      { if (countup_screen_ && !countup_state_.running) countup_screen_->on_short_press(); }
void     LcdKnob::reset_countup()      { countup_state_ = {}; if (countup_screen_) countup_screen_->mark_dirty(); }
bool     LcdKnob::is_countup_running() const { return countup_state_.running;   }
uint32_t LcdKnob::get_countup_elapsed() const { return countup_state_.elapsed_s; }

// ═══════════════════════════════════════════════════════════════════════════════
// Alarm check
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::check_alarms(uint8_t hour, uint8_t minute) {
  if (alarm1_screen_) alarm1_screen_->check(hour, minute);
  if (alarm2_screen_) alarm2_screen_->check(hour, minute);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drawing
// ═══════════════════════════════════════════════════════════════════════════════

void LcdKnob::draw_mode_dots() {
  if (in_menu_) return;
  if (groups_.empty()) return;
  const auto &g = groups_[current_group_];
  if (g.pages.size() <= 1) return;

  auto &dsp         = M5Dial.Display;
  const int n       = (int)g.pages.size();
  const int dot_r   = 4;
  const int spacing = 14;
  const int dot_y   = 212;
  const int x_start = CENTER_X - (n - 1) * spacing / 2;

  for (int i = 0; i < n; i++) {
    // Dark halo for visibility on top of any background (incl. album art)
    dsp.fillCircle(x_start + i * spacing, dot_y, dot_r + 1, COL_BLACK);
    uint16_t color = ((size_t)i == g.current_page) ? COL_ORANGE : COL_GREY_CC;
    dsp.fillCircle(x_start + i * spacing, dot_y, dot_r, color);
  }
}

}  // namespace lcd_knob
}  // namespace esphome
