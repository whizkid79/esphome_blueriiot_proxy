import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import BlueRiiotProxy


CONF_BLUERIIOT_PROXY_ID = "blueriiot_proxy_id"

TEXT_SENSOR_KEYS = (
    "raw_hex",
    "status",
    "last_error",
    "ph_status",
    "orp_status",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLUERIIOT_PROXY_ID): cv.use_id(BlueRiiotProxy),
        **{cv.Optional(key): text_sensor.text_sensor_schema() for key in TEXT_SENSOR_KEYS},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BLUERIIOT_PROXY_ID])
    for key in TEXT_SENSOR_KEYS:
        if key not in config:
            continue
        sens = await text_sensor.new_text_sensor(config[key])
        cg.add(getattr(parent, f"set_{key}_text_sensor")(sens))
