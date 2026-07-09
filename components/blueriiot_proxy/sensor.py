import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from . import BlueRiiotProxy


CONF_BLUERIIOT_PROXY_ID = "blueriiot_proxy_id"

SENSOR_KEYS = (
    "temperature",
    "ph",
    "orp",
    "temperature_local",
    "ph_local",
    "orp_local",
    "temperature_api",
    "ph_api",
    "orp_api",
    "battery",
    "raw_length",
    "raw_frame",
    "raw_temperature",
    "raw_ph",
    "raw_orp",
    "raw_conductivity",
    "raw_battery_mv",
    "raw_extra",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLUERIIOT_PROXY_ID): cv.use_id(BlueRiiotProxy),
        **{cv.Optional(key): sensor.sensor_schema() for key in SENSOR_KEYS},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BLUERIIOT_PROXY_ID])
    for key in SENSOR_KEYS:
        if key not in config:
            continue
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(parent, f"set_{key}_sensor")(sens))
