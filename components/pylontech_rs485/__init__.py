import logging

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_PYLONTECH_ID = "pylontech_id"
CONF_BATTERY = "battery"

pylontech_ns = cg.esphome_ns.namespace("pylon_rs485")
PylonRS485 = pylontech_ns.class_(
    "PylonRS485", cg.PollingComponent, uart.UARTDevice
)
PylontechBattery = pylontech_ns.class_("PylontechBattery")

CV_NUM_BATTERIES = cv.int_range(1, 16)

PYLONTECH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PYLONTECH_ID): cv.use_id(PylonRS485),
        cv.Required(CONF_BATTERY): CV_NUM_BATTERIES,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PylonRS485),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_BATTERY in config:
        cg.add(var.set_battery_count(config[CONF_BATTERY]))
