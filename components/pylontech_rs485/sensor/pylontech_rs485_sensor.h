#pragma once

#include "../pylontech_rs485.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pylon_rs485 {

class PylontechSensor : public PylontechRS485Listener, public Component {
 public:
  explicit PylontechSensor(int8_t bat_num);
  void dump_config() override;

  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(temperature)
  SUB_SENSOR(temperature_low)
  SUB_SENSOR(temperature_high)
  SUB_SENSOR(voltage_low)
  SUB_SENSOR(voltage_high)
  SUB_SENSOR(voltage_diff)
  SUB_SENSOR(soc)
  SUB_SENSOR(mos_temperature)

  void add_cell_voltage_sensor(sensor::Sensor *sensor) { this->cell_voltage_sensors_.push_back(sensor); }

  void on_line_read(const std::vector<PylonRS485_Battery> &data) override;

 protected:
  int8_t bat_num_;
  std::vector<sensor::Sensor *> cell_voltage_sensors_;
};

}  // namespace pylon_rs485
}  // namespace esphome
