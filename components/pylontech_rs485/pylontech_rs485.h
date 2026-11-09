#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pylon_rs485 {

class PylonRS485_Battery {
	public:
		uint8_t protocolVersion;
		std::vector<uint8_t> manufacturerInfo;
		std::vector<uint16_t> cellVoltages;
		std::vector<uint16_t> cellTemperatures;
		uint16_t bmsTemperature;
		int32_t current;
		uint16_t voltage = 0;
		uint16_t currentCapacity;
		uint16_t totalCapacity;
		uint16_t cycleNumber;
		uint16_t mos_temperature;
};

class PylontechRS485Listener {
 public:
  virtual void on_line_read(const std::vector<PylonRS485_Battery> &batteries);
  virtual void dump_config();
};

class PylonRS485 : public PollingComponent, public uart::UARTDevice {
	public:
		PylonRS485() {}
		void setup() override {}
		void loop() override;
		void update() override;

		void set_battery_count(uint8_t count) { if (count > battery_count_) battery_count_ = count; }

		std::vector<PylonRS485_Battery> battery_modules;

		void register_listener(PylontechRS485Listener *listener) { this->listeners_.push_back(listener); }
	
	private:
		// Upper bound on how many bytes loop() will drain from the UART in a
		// single call. Without this cap, a noisy/floating RS485 line that keeps
		// streaming bytes faster than (or as fast as) they can be read would
		// keep available() true forever, causing loop() to never return and
		// starving the rest of the firmware (WiFi, API, logger, ...).
		static constexpr int MAX_BYTES_PER_LOOP = 256;

		enum class State { IDLE, SEND_REQUEST, WAITING_FOR_RESPONSE };

		uint8_t battery_count_{1};
		State state_{State::IDLE};
		uint8_t current_battery_id_{2};
		uint32_t request_start_{0};
		uint8_t retry_count_{0};
		std::vector<uint8_t> rx_buffer_;
		std::vector<uint8_t> last_sent_frame_;
		std::vector<PylonRS485_Battery> batteries_;

		std::vector<uint8_t> constructFrame(uint8_t addr, uint8_t cid2, std::vector<uint8_t> data);
		bool validateResponse(const std::vector<uint8_t>& response);
		uint16_t calculateCRC(const std::vector<uint8_t> &data);
		uint16_t calculateLength(uint16_t length);
		std::vector<uint8_t> parseResponse(std::vector<uint8_t> response);
		PylonRS485_Battery parseAnalogResponse(std::vector<uint8_t> data);
		void advance_to_next_battery_();
		void retry_or_skip_(const char *reason);

		std::vector<PylontechRS485Listener *> listeners_{};
	};

} // namespace pylon_rs485
} // namespace esphome
