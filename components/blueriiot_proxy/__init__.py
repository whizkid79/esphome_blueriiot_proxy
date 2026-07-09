import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID


AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

blueriiot_proxy_ns = cg.esphome_ns.namespace("blueriiot_proxy")
BlueRiiotProxy = blueriiot_proxy_ns.class_("BlueRiiotProxy", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BlueRiiotProxy),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
