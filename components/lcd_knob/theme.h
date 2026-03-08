#pragma once
#include <cstdint>

// ════════════════════════════════════════════════════════════════════════════
//  THEME — Nordic Steel
//
//  Single source of truth for all visual design decisions.
//  Edit only this file to restyle the entire UI.
//
//  Palette: B&O-inspired. Deep navy-black ground, steel-blue accent.
//  The blue CNC frame and the display read as one continuous object.
// ════════════════════════════════════════════════════════════════════════════

namespace esphome {
namespace lcd_knob {

// ── Colour palette (RGB565) ───────────────────────────────────────────────────
static constexpr uint16_t COL_BG      = 0x0883;  // #0C1018  deep navy-black
static constexpr uint16_t COL_ACCENT  = 0x7D79;  // #7AAFC8  steel blue
static constexpr uint16_t COL_WHITE   = 0xDF5E;  // #DDE8F0  cool white
static constexpr uint16_t COL_GREY_CC = 0x7495;  // #7090A8  cool mid-slate
static constexpr uint16_t COL_GREY_55 = 0x2188;  // #243040  dark slate
static constexpr uint16_t COL_GREY_44 = 0x1906;  // #182030  very dark navy
static constexpr uint16_t COL_GREY_33 = 0x08C4;  // #0E1820  near-black navy
static constexpr uint16_t COL_BLACK   = 0x0000;  // #000000  art-overlay black

// Alias so existing code using COL_ORANGE compiles unchanged
static constexpr uint16_t COL_ORANGE  = COL_ACCENT;

// ── Typography ────────────────────────────────────────────────────────────────
// Change one line here to refont the entire UI.
#define FONT_SMALL   (&fonts::FreeSansBold9pt7b)   // labels, hints, status text
#define FONT_MEDIUM  (&fonts::FreeSansBold12pt7b)  // list items, subtitles
#define FONT_LARGE   (&fonts::FreeSansBold18pt7b)  // primary values (time, %)

// ── Arc geometry ──────────────────────────────────────────────────────────────
// 270° sweep with a gap at the bottom. Starts at 135°, full ring ends at 405°.
static constexpr int   ARC_START      = 135;
static constexpr int   ARC_END_FULL   = 405;     // ARC_START + 270
static constexpr float ARC_SWEEP      = 270.0f;

// Standard arc ring — light, timer, meater
static constexpr int   ARC_OUTER      = 105;
static constexpr int   ARC_INNER      = 97;

// Volume arc ring — slightly wider for emphasis
static constexpr int   ARC_VOL_OUTER  = 108;
static constexpr int   ARC_VOL_INNER  = 96;

// Sonos outer decoration ring
static constexpr int   ARC_DECO_OUTER = 119;
static constexpr int   ARC_DECO_INNER = 117;

// Volume cap-dot (needle tip on the arc)
static constexpr float ARC_CAP_DIST   = 102.0f;  // px from center to cap center
static constexpr int   ARC_CAP_HALO   = 7;       // dark halo radius
static constexpr int   ARC_CAP_DOT    = 5;       // bright dot radius

// ── Display geometry ──────────────────────────────────────────────────────────
static constexpr int CENTER_X         = 120;
static constexpr int CENTER_Y         = 120;

}  // namespace lcd_knob
}  // namespace esphome
