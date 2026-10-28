"""LVGL icon sweep animation component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.components.image as image
from esphome.components.lvgl.types import lv_obj_t_ptr
from esphome.const import CONF_ID
from esphome.core import TimePeriod

DEPENDENCIES = ["lvgl"]

esp_icon_sweep_ns = cg.esphome_ns.namespace("esp_icon_sweep")
EspIconSweep = esp_icon_sweep_ns.class_(
    "EspIconSweep", cg.Component
)

CONF_WIDGET = "widget"
CONF_SWEEP = "sweep"
CONF_ANIMATION_WIDTH = "animation_width"
CONF_EFFECTS = "effects"
CONF_ICONS = "icons"
CONF_IMAGE = "image"

def validate_pause(value):
    # ESPHome's positive_time_period_milliseconds rejects zero, while pause=0ms
    # is valid. Handle common zero-duration spellings explicitly.
    if isinstance(value, str) and value.strip().lower() in ("0", "0ms", "0s", "0us", "0µs", "0min", "0h"):
        return TimePeriod(milliseconds=0)
    return cv.positive_time_period_milliseconds(value)

def validate_effect(config):
    # Jump can perform multiple hops during one animation cycle.
    if config["type"] == "jump":
        config.setdefault("bounces", 2)
    elif "bounces" in config:
        raise cv.Invalid("bounces can only be used with type: jump")

    # Choose sensible defaults per effect instead of sharing scale's 100% default.
    if "from" not in config:
        config["from"] = 100 if config["type"] == "scale" else 0
    if "to" not in config:
        config["to"] = 360 if config["type"] == "rotate" else 140

    if config["type"] == "color":
        if "colors" not in config or len(config["colors"]) < 2:
            raise cv.Invalid("type: color requires at least two colors")
    elif "colors" in config:
        raise cv.Invalid("colors can only be used with type: color")

    if config["type"] == "icon":
        if CONF_ICONS not in config or len(config[CONF_ICONS]) < 2:
            raise cv.Invalid("type: icon requires at least two icons")
    elif CONF_ICONS in config:
        raise cv.Invalid("icons can only be used with type: icon")
    return config


EFFECT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required("type"): cv.one_of("scale", "rotate", "jump", "color", "icon", lower=True),
            cv.Optional("animation", default="500ms"): cv.positive_time_period_milliseconds,
            cv.Optional("alternate", default=False): cv.boolean,
            cv.Optional("pause", default="0ms"): validate_pause,
            cv.Optional("from"): cv.int_range(min=-3000, max=3000),
            cv.Optional("to"): cv.int_range(min=-3000, max=3000),
            cv.Optional("height", default=24): cv.int_range(min=-512, max=512),
            cv.Optional("bounces"): cv.int_range(min=1, max=10),
            cv.Optional("colors"): cv.All(cv.ensure_list(cv.hex_uint32_t), cv.Length(min=2, max=8)),
            cv.Optional(CONF_ICONS): cv.All(
                cv.ensure_list(cv.Schema({cv.Required(CONF_IMAGE): cv.use_id(image.Image_)})),
                cv.Length(min=2, max=8),
            ),
        }
    ),
    validate_effect,
)

ANIMATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EspIconSweep),
        cv.Required(CONF_WIDGET): cv.use_id(lv_obj_t_ptr),
        cv.Optional(CONF_SWEEP, default="8s"): cv.positive_time_period_milliseconds,
        # Rightmost X coordinate reached by the icon's left edge.
        cv.Optional(CONF_ANIMATION_WIDTH, default=800): cv.int_range(min=1, max=4096),
        # Home position X; each cycle blinks here before moving with the trail.
        cv.Optional("start_x", default=0): cv.int_range(min=-4096, max=4096),
        cv.Optional("blink_count", default=3): cv.int_range(min=0, max=20),
        # One full on/off blink cycle.
        cv.Optional("blink_transition", default="300ms"): cv.positive_time_period_milliseconds,
        cv.Optional("fade_out_transition", default="1s"): cv.positive_time_period_milliseconds,
        cv.Optional("fade_in_transition", default="500ms"): cv.positive_time_period_milliseconds,
        # Opacity percentage 0..100.
        cv.Optional("home_opacity", default=25): cv.int_range(min=0, max=100),
        cv.Optional("trail_length", default=240): cv.int_range(min=1, max=800),
        cv.Optional("trail_width", default=8): cv.int_range(min=1, max=64),
        cv.Optional("trail_color", default=0xFFFFFF): cv.hex_uint32_t,
        cv.Optional(CONF_EFFECTS, default=[]): cv.All(cv.ensure_list(EFFECT_SCHEMA), cv.Length(max=8)),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.ensure_list(ANIMATION_SCHEMA)


async def to_code(config):
    for item in config:
        var = cg.new_Pvariable(item[CONF_ID])
        await cg.register_component(var, item)

        widget = await cg.get_variable(item[CONF_WIDGET])
        cg.add(var.set_widget(widget))
        cg.add(var.set_sweep(item[CONF_SWEEP].total_milliseconds))
        cg.add(var.set_animation_width(item[CONF_ANIMATION_WIDTH]))
        cg.add(var.set_start_x(item["start_x"]))
        cg.add(var.set_blink_count(item["blink_count"]))
        cg.add(var.set_blink_transition(item["blink_transition"].total_milliseconds))
        cg.add(var.set_fade_out_transition(item["fade_out_transition"].total_milliseconds))
        cg.add(var.set_fade_in_transition(item["fade_in_transition"].total_milliseconds))
        cg.add(var.set_home_opacity(item["home_opacity"]))
        cg.add(var.set_trail_length(item["trail_length"]))
        cg.add(var.set_trail_width(item["trail_width"]))
        cg.add(var.set_trail_color(item["trail_color"]))
        color_effect_count = 0
        icon_effect_count = 0
        for effect_index, effect in enumerate(item[CONF_EFFECTS]):
            effect_type = {"scale": 0, "rotate": 1, "jump": 2, "color": 3, "icon": 4}[effect["type"]]
            colors = list(effect.get("colors", []))
            if effect["type"] == "color":
                color_effect_count += 1
                if color_effect_count > 1:
                    raise cv.Invalid("Only one type: color effect can be configured per esp_icon_sweep")
            if effect["type"] == "icon":
                icon_effect_count += 1
                if icon_effect_count > 1:
                    raise cv.Invalid("Only one type: icon effect can be configured per esp_icon_sweep")
            colors = (colors + [0xFFFFFF] * 8)[:8]
            cg.add(var.add_effect(
                effect_type,
                effect["animation"].total_milliseconds,
                effect["pause"].total_milliseconds,
                effect["from"],
                effect["to"],
                effect["height"],
                effect["alternate"],
                effect.get("bounces", 1),
                len(effect.get("colors", [])),
                *colors,
            ))
            for icon_index, icon_config in enumerate(effect.get(CONF_ICONS, [])):
                icon_var = await cg.get_variable(icon_config[CONF_IMAGE])
                cg.add(var.add_icon(effect_index, icon_index, icon_var.get_lv_image_dsc()))
