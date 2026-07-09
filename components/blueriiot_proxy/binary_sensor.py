import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import BlueRiiotProxy


CONF_BLUERIIOT_PROXY_ID = "blueriiot_proxy_id"

BINARY_SENSOR_KEYS = ("raw_packet_valid",)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLUERIIOT_PROXY_ID): cv.use_id(BlueRiiotProxy),
        **{cv.Optional(key): binary_sensor.binary_sensor_schema() for key in BINARY_SENSOR_KEYS},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BLUERIIOT_PROXY_ID])
    for key in BINARY_SENSOR_KEYS:
        if key not in config:
            continue
        sens = await binary_sensor.new_binary_sensor(config[key])
        cg.add(getattr(parent, f"set_{key}_binary_sensor")(sens))
