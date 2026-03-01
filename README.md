# M5Stack Dial — Sonos Playlist Controller

An [ESPHome](https://esphome.io) firmware for the **M5Stack Dial** that turns the device into a physical Sonos controller, using your Sonos favourites list as a playlist menu.

> **ESPHome ≥ 2024.2 required** (LVGL display framework support)

---

## What this is

The M5Stack Dial sits on your desk or wall.  Turn the encoder to browse your Sonos favourites, press to play with shuffle.  Long-press the main button to cycle between three modes:

| Mode | Display | Encoder | Short press |
|------|---------|---------|-------------|
| **Playlist** | Favourite name + index | Browse favourites | Play selected + shuffle |
| **Now Playing** | Track title + artist, play state | Skip ▶▶ / ◀◀ | Play/pause |
| **Volume** | Arc + percentage | ±2 % per click | Play/pause |

The Sonos favourites list is pulled **automatically** from Home Assistant — no manual configuration beyond pointing the device at your media player entity.

---

## Hardware required

**Exactly: M5Stack Dial** (model M5DIAL-S3)

- ESP32-S3, 8 MB flash
- GC9A01A 240×240 round SPI display
- EC11 rotary encoder with push button
- PCF8563 hardware RTC
- RC522 NFC reader (I2C, present on board — initialised but not used by this firmware)
- Passive buzzer

This firmware will **not** work on M5Stack Core, M5Stick, M5Atom, or any other M5Stack product — the pin assignments are specific to the Dial.

---

## Getting started

### 1. Prerequisites

- Home Assistant with the [ESPHome add-on](https://esphome.io/guides/getting_started_hassio.html) installed, **or** the [ESPHome CLI](https://esphome.io/guides/getting_started_command_line.html) on your computer.
- Sonos already integrated in Home Assistant (Settings → Devices & Services → Add integration → Sonos).
- USB-C cable for first flash.

### 2. Find your Sonos entity ID

1. In Home Assistant go to **Settings → Devices & Services → Entities**.
2. Filter by "media_player" and locate your Sonos speaker (e.g. `media_player.living_room`).
3. Note the entity ID — you will put it in the substitution below.

### 3. Configure

Clone or download this repository, then:

```bash
cp secrets.yaml.example secrets.yaml
```

Edit `secrets.yaml`:

```yaml
wifi_ssid: "MyNetwork"
wifi_password: "s3cr3t"
ota_password: "choose_a_password"
api_encryption_key: "base64_32_byte_key"   # generate: esphome config generate-key
```

Edit the `substitutions` block at the top of `m5dial-sonos.yaml`:

```yaml
substitutions:
  sonos_entity: "media_player.living_room"   # ← your entity ID here
  timezone: "Europe/Copenhagen"              # ← your IANA timezone
```

Everything else can stay at its default value.

### 4. Flash

**Via ESPHome Dashboard (recommended for first flash)**

1. Copy `m5dial-sonos.yaml` and `secrets.yaml` into your ESPHome config folder.
2. Open the Dashboard → click the three-dot menu on the new device → **Install → Plug into this computer**.
3. Follow the on-screen steps.

**Via CLI**

```bash
# First flash over USB
esphome run m5dial-sonos.yaml

# Subsequent updates over Wi-Fi (OTA)
esphome run m5dial-sonos.yaml --device <ip-address>
```

---

## Modes explained

```
Long press (≥ 800 ms) on the main button → cycle mode
Short press  (< 800 ms)                  → mode action
```

```
MODE 0 — PLAYLIST
┌──────────────────────────┐
│           ♫              │  ← music note icon
│                          │
│     Jazz Classics        │  ← favourite name (scrolls if long)
│                          │
│         2 / 7            │  ← index / total
│                     ● ○ ○│  ← mode indicator dots
└──────────────────────────┘
  Rotate: browse   Press: play + shuffle

MODE 1 — NOW PLAYING
┌──────────────────────────┐
│           ▶              │  ← live play/pause icon
│                          │
│   So What                │  ← track title (auto-scrolls)
│   Miles Davis            │  ← artist name
│                     ○ ● ○│
└──────────────────────────┘
  Rotate: skip tracks   Press: play/pause

MODE 2 — VOLUME
┌──────────────────────────┐
│         Volume           │
│  ╔═══════════════════╗   │
│  ║       45%         ║   │  ← arc + percentage
│  ╚═══════════════════╝   │
│                     ○ ○ ●│
└──────────────────────────┘
  Rotate: ±2 %   Press: play/pause
```

**Screensaver:** after 30 seconds of inactivity the backlight dims to 10 %.  Any encoder turn or button press wakes it instantly.

---

## How Sonos favourites are pulled

The firmware subscribes to the `source_list` attribute of your `sonos_entity` via the Home Assistant native API.  This attribute contains all your Sonos favourites and is updated by HA in real time whenever you add or remove items in the Sonos app.

No extra YAML configuration of the favourites is needed — just point the device at the right entity ID and the list appears automatically on the dial.

---

## Troubleshooting

### `source_list` is empty / "No favourites found" on the display

1. Check that you have favourites saved in the Sonos app (heart/save icon on a station or playlist).
2. Confirm the `sonos_entity` substitution matches exactly (copy-paste from HA Entities page — it is case-sensitive).
3. Open ESPHome logs (`esphome logs m5dial-sonos.yaml`) and look for lines tagged `[sonos]`.  You should see `"Parsed N playlists from source_list"`.
4. Make sure `homeassistant_states: true` is present in the `api:` block — without it, attribute subscriptions will silently do nothing.

### Display shows inverted colours or wrong colours

The GC9A01A on the M5Stack Dial requires `color_order: bgr` and `invert_colors: true`.  Both are set in the config.  If you still see inverted colours, double-check that no other include or package is overriding these settings.

### Button timing feels wrong

Adjust the `long_press_duration` substitution (default `800ms`).  Lower values (e.g. `600ms`) make mode changes easier to trigger; higher values (e.g. `1000ms`) require a more deliberate hold.

### RTC shows the wrong time on first boot

On first boot the RTC syncs from Home Assistant.  If HA is not yet connected, the RTC may briefly show 2000-01-01.  Once the API connects, time syncs automatically and is written to the hardware RTC.  Check that the `timezone` substitution is a valid IANA timezone string (e.g. `"America/New_York"`).

### Build error about missing fonts

ESPHome downloads Montserrat from Google Fonts on first build — internet access is required.  If your build environment is offline, download [Montserrat-Regular.ttf](https://fonts.google.com/specimen/Montserrat), place it in a `fonts/` folder next to this YAML, and change the `file:` paths in the `font:` section from `"gfonts://Montserrat-Regular"` to `"fonts/Montserrat-Regular.ttf"`.

### OTA update fails

1. Confirm the device is reachable: `ping <ip-address>`.
2. Verify `ota_password` in `secrets.yaml` matches the value used during the initial flash.
3. Try re-flashing over USB if OTA is completely broken.

---

## File overview

| File | Purpose |
|------|---------|
| `m5dial-sonos.yaml` | Main ESPHome configuration |
| `secrets.yaml.example` | Template for credentials — copy to `secrets.yaml` |
| `ha_automation_example.yaml` | Optional HA automations that react to dial activity |
| `.gitignore` | Keeps `secrets.yaml` and build artefacts out of git |

---

## Contributing

Issues and pull requests welcome.  When reporting a bug please include:

- ESPHome version (`esphome version`)
- Relevant lines from the ESPHome logs
- What you expected vs what happened
