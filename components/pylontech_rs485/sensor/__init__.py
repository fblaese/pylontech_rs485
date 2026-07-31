import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_CURRENT,
    CONF_ID,
    CONF_NAME,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from .. import CONF_BATTERY, CONF_PYLONTECH_ID, PYLONTECH_COMPONENT_SCHEMA, pylontech_ns

PylontechSensor = pylontech_ns.class_("PylontechSensor", cg.Component)

CONF_SOC = "soc"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_VOLTAGE_LOW = "voltage_low"
CONF_VOLTAGE_HIGH = "voltage_high"
CONF_VOLTAGE_DIFF = "voltage_diff"
CONF_CELL_VOLTAGES = "cell_voltages"
CONF_MOS_TEMPERATURE = "mos_temperature"

TYPES: dict[str, cv.Schema] = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_VOLTAGE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_VOLTAGE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_VOLTAGE_DIFF: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_SOC: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_BATTERY,
    ),
    CONF_MOS_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
}

CELL_COUNT = 15

CELL_VOLTAGES_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
)

CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(PylontechSensor)}
).extend({cv.Optional(marker): schema for marker, schema in TYPES.items()}).extend(
    {cv.Optional(CONF_CELL_VOLTAGES): CELL_VOLTAGES_SCHEMA}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    for marker in TYPES:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    if cell_config := config.get(CONF_CELL_VOLTAGES):
        base_name = cell_config.get(CONF_NAME, "Cell Voltage")
        for i in range(1, CELL_COUNT + 1):
            cell_id = ID(
                f"{config[CONF_ID].id}_cell_{i}",
                is_declaration=True,
                type=sensor.Sensor,
            )
            sens = await sensor.new_sensor({**cell_config, CONF_ID: cell_id, CONF_NAME: f"{base_name} {i:02}"})
            cg.add(bat.add_cell_voltage_sensor(sens))

    cg.add(paren.register_listener(bat))
    cg.add(paren.set_battery_count(config[CONF_BATTERY]))
