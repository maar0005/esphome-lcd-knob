"""ESPHome custom component: Waveshare LCD Knob controller (modular screen edition).

Screens are declared as a list; each has a type, a used flag, and type-specific
entity configuration.  Only screens with  used: true  are compiled in.

Example configuration:

  lcd_knob:
    id: knob
    screen_off_time: 30000
    screens:
      - type: sonos
        used: true
        entity: media_player.living_room
        volume_step: 2
      - type: meater
        used: false
        entity_temperature: sensor.meater_probe_1_internal_temperature
        entity_target:      sensor.meater_probe_1_target_temperature
        entity_ambient:     sensor.meater_probe_1_ambient_temperature
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import include_builtin_idf_component

CODEOWNERS = ["@mtn"]
DEPENDENCIES = []
AUTO_LOAD = []

# ── Config key constants ───────────────────────────────────────────────────────
CONF_SCREENS             = "screens"
CONF_TYPE                = "type"
CONF_USED                = "used"
CONF_ENTITY              = "entity"
CONF_VOLUME_STEP         = "volume_step"
CONF_ENTITY_TEMPERATURE  = "entity_temperature"
CONF_ENTITY_TARGET       = "entity_target"
CONF_ENTITY_AMBIENT      = "entity_ambient"
CONF_SCREEN_OFF_TIME     = "screen_off_time"
CONF_LONG_PRESS_DURATION = "long_press_duration"
CONF_DEFAULT_DURATION    = "default_duration"
CONF_LIGHT_NAME          = "name"

# ── C++ class references ──────────────────────────────────────────────────────
lcd_knob_ns = cg.esphome_ns.namespace("lcd_knob")
LcdKnob = lcd_knob_ns.class_("LcdKnob", cg.Component)

# ── Per-type schemas ──────────────────────────────────────────────────────────
SONOS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY): cv.string,
        cv.Optional(CONF_VOLUME_STEP, default=2): cv.int_range(min=1, max=10),
    }
)

MEATER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_TEMPERATURE): cv.string,
        cv.Optional(CONF_ENTITY_TARGET,  default=""): cv.string,
        cv.Optional(CONF_ENTITY_AMBIENT, default=""): cv.string,
    }
)

TIMER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DEFAULT_DURATION, default=5): cv.int_range(min=1, max=99),
    }
)

ALARM_SCHEMA   = cv.Schema({})
COUNTUP_SCHEMA = cv.Schema({})

LIGHT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY): cv.string,
        # Optional display name.  If omitted the entity ID tail is used,
        # e.g. "light.kitchen_ceiling" → "KITCHEN CEILING".
        cv.Optional(CONF_LIGHT_NAME, default=""): cv.string,
    }
)

SCREEN_TYPE_SCHEMAS = {
    "sonos":   SONOS_SCHEMA,
    "meater":  MEATER_SCHEMA,
    "timer":   TIMER_SCHEMA,
    "timer2":  TIMER_SCHEMA,
    "alarm":   ALARM_SCHEMA,
    "alarm2":  ALARM_SCHEMA,
    "countup": COUNTUP_SCHEMA,
    "light":   LIGHT_SCHEMA,
}


def validate_screen(value):
    """Validate a single screen entry, applying the correct per-type schema."""
    if not isinstance(value, dict):
        raise cv.Invalid("Each screen must be a mapping")
    t = value.get(CONF_TYPE)
    if t not in SCREEN_TYPE_SCHEMAS:
        raise cv.Invalid(
            f"Unknown screen type '{t}'. Valid types: {sorted(SCREEN_TYPE_SCHEMAS)}"
        )
    base_schema = cv.Schema(
        {
            cv.Required(CONF_TYPE): cv.one_of(*SCREEN_TYPE_SCHEMAS, lower=True),
            cv.Optional(CONF_USED, default=True): cv.boolean,
        },
        extra=cv.ALLOW_EXTRA,
    )
    # Merge base and type-specific validation results
    base_keys = {CONF_TYPE, CONF_USED}
    type_value = {k: v for k, v in value.items() if k not in base_keys}
    validated = {**base_schema(value), **SCREEN_TYPE_SCHEMAS[t](type_value)}
    return validated


# ── Top-level component schema ────────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LcdKnob),
        cv.Optional(CONF_SCREEN_OFF_TIME,     default=30000): cv.positive_int,
        cv.Optional(CONF_LONG_PRESS_DURATION, default=800):   cv.positive_int,
        cv.Required(CONF_SCREENS): cv.ensure_list(validate_screen),
    }
).extend(cv.COMPONENT_SCHEMA)


# ── Code generation ───────────────────────────────────────────────────────────
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_screen_off_time(config[CONF_SCREEN_OFF_TIME]))
    cg.add(var.set_long_press_duration(config[CONF_LONG_PRESS_DURATION]))

    for screen in config[CONF_SCREENS]:
        if not screen.get(CONF_USED, True):
            continue  # skip disabled screens entirely

        t = screen[CONF_TYPE]

        if t == "sonos":
            cg.add(
                var.configure_sonos(
                    screen[CONF_ENTITY],
                    screen.get(CONF_VOLUME_STEP, 2),
                )
            )

        elif t == "meater":
            cg.add(
                var.configure_meater(
                    screen.get(CONF_ENTITY_TEMPERATURE, ""),
                    screen.get(CONF_ENTITY_TARGET,      ""),
                    screen.get(CONF_ENTITY_AMBIENT,     ""),
                )
            )

        elif t == "timer":
            cg.add(var.configure_timer(screen.get(CONF_DEFAULT_DURATION, 5) * 60))

        elif t == "timer2":
            cg.add(var.configure_timer2(screen.get(CONF_DEFAULT_DURATION, 5) * 60))

        elif t == "alarm":
            cg.add(var.configure_alarm())

        elif t == "alarm2":
            cg.add(var.configure_alarm2())

        elif t == "countup":
            cg.add(var.configure_countup())

        elif t == "light":
            entity = screen[CONF_ENTITY]
            name   = screen.get(CONF_LIGHT_NAME, "")
            if not name:
                # Derive a tidy display name from the entity ID tail.
                name = entity.split(".")[-1].replace("_", " ").upper()
            cg.add(var.configure_light(entity, name))

    # ── Arduino library dependencies ──────────────────────────────────────────
    # NOTE: M5Unified/M5Dial are used for hardware abstraction during the
    # initial port.  These will be replaced with direct LovyanGFX + GPIO
    # drivers once the Waveshare-specific bring-up is complete.
    cg.add_library("m5stack/M5Unified", "0.2.2")
    cg.add_library("m5stack/M5Dial",    "1.0.2")

    # M5GFX (bundled with M5Unified) requires these IDF components
    include_builtin_idf_component("esp_lcd")
    include_builtin_idf_component("driver")

    cg.add_platformio_option("lib_ldf_mode", "deep+")
