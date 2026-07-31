#include "pylontech_rs485_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylon_rs485 {

static const char *const TAG = "pylontech.sensor";

PylontechSensor::PylontechSensor(int8_t bat_num) : bat_num_(bat_num) {}

void PylontechSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pylontech Sensor:");
  ESP_LOGCONFIG(TAG, "  Battery Number: %d", this->bat_num_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Temperature Low", this->temperature_low_sensor_);
  LOG_SENSOR("  ", "Temperature High", this->temperature_high_sensor_);
  LOG_SENSOR("  ", "Voltage Low", this->voltage_low_sensor_);
  LOG_SENSOR("  ", "Voltage High", this->voltage_high_sensor_);
  LOG_SENSOR("  ", "Voltage Diff", this->voltage_diff_sensor_);
  LOG_SENSOR("  ", "State of Charge", this->soc_sensor_);
  LOG_SENSOR("  ", "MOS Temperature", this->mos_temperature_sensor_);
  for (size_t i = 0; i < this->cell_voltage_sensors_.size(); i++)
    LOG_SENSOR("  ", str_sprintf("Cell Voltage %d", i + 1).c_str(), this->cell_voltage_sensors_[i]);
}

void PylontechSensor::on_line_read(const std::vector<PylonRS485_Battery> &data) {
  if (this->bat_num_ <= 0 || static_cast<size_t>(this->bat_num_) > data.size()) {
    ESP_LOGW(TAG, "Invalid battery index: %d", this->bat_num_);
    return;
  }

  const auto &line = data[this->bat_num_ - 1];

  if (line.voltage == 0) {
    ESP_LOGW(TAG, "Battery voltage is zero, skipping update.");
    return;
  }

  if (this->voltage_sensor_)
    this->voltage_sensor_->publish_state(line.voltage / 1000.0f);
  if (this->current_sensor_)
    this->current_sensor_->publish_state(line.current / 1000.0f);

  // Aggregate temperature values
  if (!line.cellTemperatures.empty()) {
    uint16_t low = *std::min_element(line.cellTemperatures.begin(), line.cellTemperatures.end());
    uint16_t high = *std::max_element(line.cellTemperatures.begin(), line.cellTemperatures.end());

    if (this->temperature_low_sensor_)
      this->temperature_low_sensor_->publish_state(low / 10.0f - 273.1f);
    if (this->temperature_high_sensor_)
      this->temperature_high_sensor_->publish_state(high / 10.0f - 273.1f);
  }

  // Aggregate voltage values
  if (!line.cellVoltages.empty()) {
    uint16_t low = *std::min_element(line.cellVoltages.begin(), line.cellVoltages.end());
    uint16_t high = *std::max_element(line.cellVoltages.begin(), line.cellVoltages.end());

    if (this->voltage_low_sensor_)
      this->voltage_low_sensor_->publish_state(low / 1000.0f);
    if (this->voltage_high_sensor_)
      this->voltage_high_sensor_->publish_state(high / 1000.0f);
    if (this->voltage_diff_sensor_)
      this->voltage_diff_sensor_->publish_state((high - low) / 1000.0f);
  }

  if (this->soc_sensor_ && line.totalCapacity != 0) {
    this->soc_sensor_->publish_state(static_cast<float>(line.currentCapacity) / line.totalCapacity * 100.0f);
  }

  if (this->mos_temperature_sensor_)
    this->mos_temperature_sensor_->publish_state(line.mos_temperature / 10.0f - 273.1f);

  for (size_t i = 0; i < this->cell_voltage_sensors_.size(); i++) {
    if (i < line.cellVoltages.size())
      this->cell_voltage_sensors_[i]->publish_state(line.cellVoltages[i] / 1000.0f);
  }
}

}  // namespace pylon_rs485
}  // namespace esphome
