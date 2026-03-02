"""ESPHome custom component: M5Dial Sonos Controller.

Uses the M5Unified + M5Dial Arduino libraries (which bundle LovyanGFX)
for display rendering, while ESPHome handles Wi-Fi, HA API, rotary encoder,
and button input.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import include_builtin_idf_component

CODEOWNERS = ["@mtn"]
DEPENDENCIES = []
AUTO_LOAD = []

CONF_SONOS_ENTITY = "sonos_entity"
CONF_SCREEN_OFF_TIME = "screen_off_time"
CONF_LONG_PRESS_DURATION = "long_press_duration"
CONF_VOLUME_STEP = "volume_step"

m5dial_sonos_ns = cg.esphome_ns.namespace("m5dial_sonos")
M5DialSonos = m5dial_sonos_ns.class_("M5DialSonos", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5DialSonos),
        cv.Required(CONF_SONOS_ENTITY): cv.string,
        cv.Optional(CONF_SCREEN_OFF_TIME, default=30000): cv.positive_int,
        cv.Optional(CONF_LONG_PRESS_DURATION, default=800): cv.positive_int,
        cv.Optional(CONF_VOLUME_STEP, default=2): cv.int_range(min=1, max=10),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sonos_entity(config[CONF_SONOS_ENTITY]))
    cg.add(var.set_screen_off_time(config[CONF_SCREEN_OFF_TIME]))
    cg.add(var.set_long_press_duration(config[CONF_LONG_PRESS_DURATION]))
    cg.add(var.set_volume_step(config[CONF_VOLUME_STEP]))

    cg.add_library("m5stack/M5Unified", "0.2.2")
    cg.add_library("m5stack/M5Dial", "1.0.2")

    # M5GFX (bundled with M5Unified) requires esp_lcd and driver IDF components
    include_builtin_idf_component("esp_lcd")
    include_builtin_idf_component("driver")

    cg.add_platformio_option("lib_ldf_mode", "deep+")
