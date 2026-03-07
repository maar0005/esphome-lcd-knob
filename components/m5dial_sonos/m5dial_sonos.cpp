#include "m5dial_sonos.h"
#include "esphome/core/log.h"
#include <M5Dial.h>

namespace esphome {
namespace m5dial_sonos {

static const char *const TAG = "m5dial_sonos";

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

float M5DialSonos::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

void M5DialSonos::setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* encoder */, false /* RFID */);
  M5Dial.Display.setRotation(0);
  M5Dial.Display.fillScreen(COL_BG);
  M5Dial.Display.setBrightness(BRIGHTNESS_FULL);
  last_interaction_ = millis();

  // ── Build groups in declaration order ─────────────────────────────────────
  std::vector<std::string> group_names;
  for (auto &sc : screen_configs_) {
    ScreenGroup g;
    switch (sc.kind) {
      case ScreenKind::SONOS: {
        g.name             = "Sonos";
        sonos_state_.entity = sc.sonos_entity;
        sonos_playlist_    = new SonosPlaylistScreen(&sonos_state_);
        sonos_now_playing_ = new SonosNowPlayingScreen(&sonos_state_);
        sonos_volume_      = new SonosVolumeScreen(&sonos_state_, sc.sonos_volume_step);
        g.pages = {sonos_playlist_, sonos_now_playing_, sonos_volume_};
        break;
      }
      case ScreenKind::MEATER: {
        g.name  = "Meater";
        meater_ = new MeaterScreen();
        if (!sc.meater_entity_temp.empty())    meater_->set_entity_temperature(sc.meater_entity_temp);
        if (!sc.meater_entity_target.empty())  meater_->set_entity_target(sc.meater_entity_target);
        if (!sc.meater_entity_ambient.empty()) meater_->set_entity_ambient(sc.meater_entity_ambient);
        g.pages = {meater_};
        break;
      }
    }
    groups_.push_back(std::move(g));
    group_names.push_back(groups_.back().name);
  }

  menu_screen_ = new MenuScreen();
  menu_screen_->set_groups(group_names);

  ESP_LOGI(TAG, "M5Dial ready — %d group(s) registered", (int)groups_.size());
}

void M5DialSonos::loop() {
  M5Dial.update();

  uint32_t now = millis();

  // ── Screen dim ────────────────────────────────────────────────────────────
  bool playing = sonos_state_.is_playing;
  if (!screen_dimmed_ && !playing &&
      (now - last_interaction_) > screen_off_time_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_DIM);
    screen_dimmed_ = true;
  }

  // ── Touch double-tap → toggle menu ───────────────────────────────────────
  // CST816S reports wasPressed() once per touch-down event.
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

Screen *M5DialSonos::current_screen() const {
  if (in_menu_) return menu_screen_;
  if (groups_.empty()) return nullptr;
  const auto &g = groups_[current_group_];
  if (g.pages.empty()) return nullptr;
  return g.pages[g.current_page];
}

void M5DialSonos::next_page_in_group() {
  if (groups_.empty()) return;
  auto &g = groups_[current_group_];
  if (g.pages.size() <= 1) return;
  g.current_page = (g.current_page + 1) % g.pages.size();
  g.pages[g.current_page]->mark_dirty();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Screen declarations (pre-setup, called from generated code)
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::configure_sonos(const std::string &entity, int volume_step) {
  ScreenConfig sc;
  sc.kind              = ScreenKind::SONOS;
  sc.sonos_entity      = entity;
  sc.sonos_volume_step = volume_step;
  screen_configs_.push_back(sc);
}

void M5DialSonos::configure_meater(const std::string &t,
                                    const std::string &tgt,
                                    const std::string &amb) {
  ScreenConfig sc;
  sc.kind                  = ScreenKind::MEATER;
  sc.meater_entity_temp    = t;
  sc.meater_entity_target  = tgt;
  sc.meater_entity_ambient = amb;
  screen_configs_.push_back(sc);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Menu control
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::open_menu() {
  if (in_menu_) return;
  in_menu_ = true;
  if (menu_screen_) {
    menu_screen_->set_selection(current_group_);
    menu_screen_->mark_dirty();
  }
  wake_screen();
  ESP_LOGD(TAG, "Menu opened");
}

void M5DialSonos::close_menu() {
  if (!in_menu_) return;
  in_menu_ = false;
  Screen *s = current_screen();
  if (s) s->mark_dirty();
  ESP_LOGD(TAG, "Menu closed → group %d", (int)current_group_);
}

void M5DialSonos::toggle_menu() {
  if (in_menu_) close_menu();
  else          open_menu();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Input
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::on_rotary_cw() {
  wake_screen();
  Screen *s = current_screen();
  if (s) s->on_rotary_cw();
}

void M5DialSonos::on_rotary_ccw() {
  wake_screen();
  Screen *s = current_screen();
  if (s) s->on_rotary_ccw();
}

void M5DialSonos::on_short_press() {
  wake_screen();
  if (in_menu_) {
    // Enter the highlighted group and close menu
    if (menu_screen_) current_group_ = menu_screen_->get_selection();
    close_menu();
    return;
  }
  Screen *s = current_screen();
  if (s) s->on_short_press();
}

void M5DialSonos::on_long_press() {
  wake_screen();
  if (in_menu_) {
    close_menu();  // Cancel: long press in menu exits without changing group
    return;
  }
  next_page_in_group();
  // Beep is handled in YAML
}

void M5DialSonos::wake_screen() {
  last_interaction_ = millis();
  if (screen_dimmed_) {
    M5Dial.Display.setBrightness(BRIGHTNESS_FULL);
    screen_dimmed_ = false;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sonos state setters
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::set_playlist_json(const std::string &json) {
  if (!sonos_playlist_) return;
  sonos_state_.playlist_names.clear();

  std::string raw = json;
  size_t s = raw.find_first_not_of(" \t\r\n");
  size_t e = raw.find_last_not_of(" \t\r\n");
  if (s == std::string::npos) raw.clear();
  else raw = raw.substr(s, e - s + 1);

  if (raw.empty() || raw.front() != '[') {
    ESP_LOGW(TAG, "source_list is not a JSON array");
    sonos_playlist_->mark_dirty();
    return;
  }

  raw = raw.substr(1, raw.size() > 2 ? raw.size() - 2 : 0);

  bool in_quote = false, escape_next = false;
  std::string token;
  for (char c : raw) {
    if (escape_next) { token += c; escape_next = false; continue; }
    if (c == '\\')   { escape_next = true; continue; }
    if (c == '"') {
      if (!in_quote) { in_quote = true; token.clear(); }
      else           { if (!token.empty()) sonos_state_.playlist_names.push_back(token); in_quote = false; }
      continue;
    }
    if (in_quote) token += c;
  }

  if (sonos_state_.playlist_index >= (int)sonos_state_.playlist_names.size())
    sonos_state_.playlist_index = 0;

  sonos_playlist_->mark_dirty();
  ESP_LOGI(TAG, "Parsed %d playlists", (int)sonos_state_.playlist_names.size());
}

void M5DialSonos::set_media_title(const std::string &title) {
  if (sonos_state_.media_title == title) return;
  sonos_state_.media_title = title;
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

void M5DialSonos::set_media_artist(const std::string &artist) {
  if (sonos_state_.media_artist == artist) return;
  sonos_state_.media_artist = artist;
  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

void M5DialSonos::set_volume_level(float level) {
  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  sonos_state_.volume = level;
  if (sonos_volume_) sonos_volume_->mark_dirty();
}

void M5DialSonos::set_player_state(const std::string &state) {
  bool was_playing = sonos_state_.is_playing;
  sonos_state_.is_playing = (state == "playing");

  // Auto-jump to Now Playing when playback starts
  if (sonos_state_.is_playing && !was_playing && sonos_now_playing_) {
    for (size_t gi = 0; gi < groups_.size(); gi++) {
      auto &g = groups_[gi];
      for (size_t pi = 0; pi < g.pages.size(); pi++) {
        if (g.pages[pi] == sonos_now_playing_) {
          current_group_ = gi;
          g.current_page = pi;
          in_menu_       = false;
          break;
        }
      }
    }
    wake_screen();
  }

  if (sonos_now_playing_) sonos_now_playing_->mark_dirty();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sonos getters
// ═══════════════════════════════════════════════════════════════════════════════

float M5DialSonos::get_volume() const {
  return sonos_state_.volume;
}

int M5DialSonos::get_playlist_count() const {
  return static_cast<int>(sonos_state_.playlist_names.size());
}

std::string M5DialSonos::get_current_playlist_name() const {
  if (sonos_state_.playlist_names.empty()) return "";
  return sonos_state_.playlist_names[sonos_state_.playlist_index];
}

// ═══════════════════════════════════════════════════════════════════════════════
// Screen-type queries
// ═══════════════════════════════════════════════════════════════════════════════

bool M5DialSonos::is_sonos_playlist()    const { return !in_menu_ && current_screen() == sonos_playlist_;    }
bool M5DialSonos::is_sonos_now_playing() const { return !in_menu_ && current_screen() == sonos_now_playing_; }
bool M5DialSonos::is_sonos_volume()      const { return !in_menu_ && current_screen() == sonos_volume_;      }
bool M5DialSonos::is_meater()            const { return !in_menu_ && current_screen() == meater_;            }

// ═══════════════════════════════════════════════════════════════════════════════
// Meater state setters
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::set_meater_temperature(float t) { if (meater_) meater_->set_temperature(t); }
void M5DialSonos::set_meater_target     (float t) { if (meater_) meater_->set_target(t);      }
void M5DialSonos::set_meater_ambient    (float t) { if (meater_) meater_->set_ambient(t);     }

// ═══════════════════════════════════════════════════════════════════════════════
// Drawing
// ═══════════════════════════════════════════════════════════════════════════════

void M5DialSonos::draw_mode_dots() {
  if (in_menu_) return;  // MenuScreen draws its own dots
  if (groups_.empty()) return;

  const auto &g = groups_[current_group_];
  if (g.pages.size() <= 1) return;  // Single-page group: no dots

  auto &dsp         = M5Dial.Display;
  const int n       = static_cast<int>(g.pages.size());
  const int dot_r   = 4;
  const int spacing = 14;
  const int dot_y   = 212;
  const int x_start = CENTER_X - (n - 1) * spacing / 2;

  for (int i = 0; i < n; i++) {
    uint16_t color = (static_cast<size_t>(i) == g.current_page) ? COL_ORANGE : COL_GREY_44;
    dsp.fillCircle(x_start + i * spacing, dot_y, dot_r, color);
  }
}

}  // namespace m5dial_sonos
}  // namespace esphome
