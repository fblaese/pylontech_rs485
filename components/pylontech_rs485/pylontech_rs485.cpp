#include "pylontech_rs485.h"
#include <vector>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace esphome {
namespace pylon_rs485 {

std::vector<uint8_t> PylonRS485::constructFrame(uint8_t addr, uint8_t cid2, std::vector<uint8_t> data) {
    uint8_t version = 0x20;
    uint8_t cid1 = 0x46;
    uint8_t lchksum = 0x0;
    uint16_t len = calculateLength(data.size() * 2);

    std::vector<uint8_t> raw_frame = {version, addr, cid1, cid2};
    raw_frame.push_back((len >> 8) & 0xFF);
    raw_frame.push_back(len & 0xFF);
    raw_frame.insert(raw_frame.end(), data.begin(), data.end());

    char buffer[raw_frame.size() * 2 + 7];
    char * buffer_iterator = buffer;
    std::sprintf(buffer_iterator, "~");
    buffer_iterator++;
    for (uint8_t b : raw_frame) {
        std::sprintf(buffer_iterator, "%02X", b);
        buffer_iterator += 2;
    }

    uint16_t crc = calculateCRC(raw_frame);
    std::sprintf(buffer_iterator, "%02X%02X\r", (crc >> 8) & 0xFF, crc & 0xFF);

    ESP_LOGD("PylonRS485", "send: %s", buffer);

    std::vector<uint8_t> frame;
    for (uint8_t b : buffer) {
        if (b == '\0') break;
        frame.push_back(b);
    }
    return frame;
}

std::vector<uint8_t> PylonRS485::parseResponse(std::vector<uint8_t> response) {
    /*std::vector<uint8_t> data;

    if (response[0] != '~') return data;
    for (auto it = response.begin() + 1; it != response.end(); it++) {
        if (*it == '\r') {
            break;
        }

        data.push_back()
    }*/

    std::vector<uint8_t> output;

    // Check for framing
    if (response.size() < 3 || response.front() != '~' || response.back() != '\r') {
        ESP_LOGE("PylonRS485", "Input must start with '~' and end with '\\r'");
        return output;
    }

    // Validate even number of hex characters (excluding '~' and '\r')
    size_t hex_len = response.size() - 2;
    if (hex_len % 2 != 0) {
        ESP_LOGE("PylonRS485", "Hex payload must have even number of characters");
        return output;
    }

    // Convert ASCII hex pairs directly to bytes — no std::string/substr/stoul
    // to avoid repeated tiny heap allocations that fragment memory over time.
    output.reserve(hex_len / 2);
    for (size_t i = 1; i < response.size() - 1; i += 2) {
        auto nibble = [](uint8_t c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        output.push_back((nibble(response[i]) << 4) | nibble(response[i + 1]));
    }

    // Check CRC
    if (output.size() < 2) {
        ESP_LOGE("PylonRS485", "Input is missing CRC");
        return output;
    }
    uint16_t crc = output[output.size() - 1] | output[output.size() - 2] << 8;
    output.resize(output.size() - 2);
    uint16_t calc_crc = calculateCRC(output);
    if (crc != calc_crc) {
        ESP_LOGE("PylonRS485", "Invalid CRC");
        return {};
    }

    // TODO: check len crc

    return output;
}

PylonRS485_Battery PylonRS485::parseAnalogResponse(std::vector<uint8_t> data) {
    PylonRS485_Battery bat;
    size_t offset = 0;

    if (data.size() < offset + 1) return bat;
    uint8_t cell_count = data[offset];
    ESP_LOGD("PylonRS485", "cell_count: %d", cell_count);
    offset += 1;

    if (data.size() < offset + cell_count * 2) return bat;
    uint16_t max = 0;
    uint16_t min = UINT16_MAX;
    for (int i = 0; i < cell_count; i++) {
        uint16_t voltage = data[offset+i*2];
        voltage = (voltage << 8) | (data[offset+i*2+1]);
        bat.cellVoltages.push_back(voltage);
        ESP_LOGD("PylonRS485", "voltage: %d mV", voltage);
        if (voltage > max) max = voltage;
        if (voltage < min) min = voltage;
    }
    ESP_LOGD("PylonRS485", "max cell diff: %d mV", max - min);
    offset += cell_count * 2;

    if (data.size() < offset + 1) return bat;
    uint8_t temp_count = data[offset];
    ESP_LOGD("PylonRS485", "temp_count: %d", temp_count);
    offset += 1;

    if (data.size() < offset + temp_count * 2) return bat;
    for (int i = 0; i < temp_count; i++) {
        uint16_t temp = data[offset+i*2];
        temp = (temp << 8) | (data[offset+i*2+1]);
        if (i == 0) {
            bat.bmsTemperature = temp;
        } else {
            bat.cellTemperatures.push_back(temp);
        }
        ESP_LOGD("PylonRS485", "temp: %.1f °C", (((int16_t) temp) - 2731) / 10.0);
    }
    offset += temp_count * 2;

    if (data.size() < offset + 11) return bat;
    bat.current = (int16_t) (data[offset] << 8 | data[offset+1]);
    bat.current *= 100;
    bat.voltage = data[offset+2] << 8 | data[offset+3];
    bat.currentCapacity = data[offset+4] << 8 | data[offset+5];
    bat.totalCapacity = data[offset+7] << 8 | data[offset+8];
    bat.cycleNumber = data[offset+9] << 8 | data[offset+10];

    ESP_LOGD("PylonRS485", "current: %d mA", bat.current);
    ESP_LOGD("PylonRS485", "voltage: %d mV", bat.voltage);
    ESP_LOGD("PylonRS485", "currCapacity:  %d mAh", bat.currentCapacity);
    ESP_LOGD("PylonRS485", "totalCapacity: %d mAh", bat.totalCapacity);
    ESP_LOGD("PylonRS485", "soc: %.2f %%", (double) bat.currentCapacity / bat.totalCapacity * 100);
    ESP_LOGD("PylonRS485", "cycles: %d", bat.cycleNumber);

    return bat;
}

bool PylonRS485::validateResponse(const std::vector<uint8_t>& response) {
    char buf[response.size() + 1];
    size_t i = 0;
    for (char c : response) {
        buf[i++] = c;
    }
    buf[i] = '\0';

    if (response.size() < 4) return false;
    uint16_t receivedCRC = (response[response.size() - 1] << 8) | response[response.size() - 2];
    std::vector<uint8_t> data(response.begin(), response.end() - 2);
    return false;
    //return receivedCRC == calculateCRC(data);
}

uint16_t PylonRS485::calculateLength(uint16_t length) {
    uint16_t l;
    uint8_t lchksum;
    lchksum = (length & 0xF) + ((length > 4) & 0xF) + ((length > 8) & 0xF);
    lchksum %= 16;
    lchksum = ~lchksum;
    lchksum += 1;

    l = lchksum;
    l = (length & 0X0FFF) | ((l << 12) & 0xF000);

    return l;
}

uint16_t PylonRS485::calculateCRC(const std::vector<uint8_t> &data) {
    size_t sum = 0x0;
    uint16_t crc = 0x0;

    for (uint8_t c : data) {
        char byte[3];
        sprintf(byte, "%02X", c);
        sum += byte[0] + byte[1];
    }

    crc = sum % 0x10000;
    crc = ~crc;
    crc += 1;
    return crc;
}

void PylonRS485::update() {
    if (this->state_ != State::IDLE) {
        ESP_LOGW("PylonRS485", "update() called while poll still in progress, skipping");
        return;
    }
    ESP_LOGD("PylonRS485", "Starting poll cycle");
    this->batteries_.clear();
    this->current_battery_id_ = 2;
    this->state_ = State::SEND_REQUEST;
}

void PylonRS485::loop() {
    switch (this->state_) {
        case State::IDLE:
            return;

        case State::SEND_REQUEST: {
            // Drain any stale bytes from a previous response. Cap the number of
            // bytes drained per loop() call so a noisy/floating RS485 line that
            // keeps streaming garbage can never starve the rest of the firmware
            // (WiFi, API, logger, ...) by looping here forever.
            for (int i = 0; i < MAX_BYTES_PER_LOOP && available(); i++) {
                read();
            }
            this->rx_buffer_.clear();
            std::vector<uint8_t> frame = constructFrame(this->current_battery_id_, 0x42, {this->current_battery_id_});
            this->last_sent_frame_ = frame;
            write_array(frame);
            this->request_start_ = millis();
            this->state_ = State::WAITING_FOR_RESPONSE;
            return;
        }

        case State::WAITING_FOR_RESPONSE: {
            // Collect bytes as they arrive — no blocking. Cap the number of bytes
            // processed per loop() call for the same reason as above: a
            // continuously noisy line must not be able to monopolize the CPU.
            for (int i = 0; i < MAX_BYTES_PER_LOOP && available(); i++) {
                uint8_t byte = read();
                this->rx_buffer_.push_back(byte);
                if (byte != '\r')
                    continue;

                // The line can be noisy (reflections/missing termination on some
                // battery modules), so bytes preceding the actual '~' may have
                // already ended up in rx_buffer_. Find the start of the frame
                // that ends with this '\r' instead of assuming it starts at
                // offset 0, and drop anything that came before it.
                auto cr_pos = this->rx_buffer_.end() - 1;
                auto tilde_rit = std::find(std::vector<uint8_t>::reverse_iterator(cr_pos), this->rx_buffer_.rend(),
                                            static_cast<uint8_t>('~'));
                if (tilde_rit == this->rx_buffer_.rend()) {
                    // No frame start found before this '\r' -- pure noise, discard it and keep waiting.
                    ESP_LOGD("PylonRS485", "Discarding noise without a frame start");
                    this->rx_buffer_.clear();
                    continue;
                }
                auto tilde_pos = tilde_rit.base() - 1;
                std::vector<uint8_t> candidate(tilde_pos, this->rx_buffer_.end());

                // Some battery/transceiver combinations echo the request frame back
                // onto the (half-duplex) bus before the real reply arrives. Detect
                // and ignore that instead of treating it as the response.
                if (candidate == this->last_sent_frame_) {
                    ESP_LOGD("PylonRS485", "Ignoring echoed request frame");
                    this->rx_buffer_.clear();
                    continue;
                }

                // Full response received — parse it
                std::vector<uint8_t> response = parseResponse(candidate);
                if (response.size() > 8 && response[1] == this->current_battery_id_) {
                    std::vector<uint8_t> analogData(response.begin() + 8, response.end());
                    PylonRS485_Battery bat = parseAnalogResponse(analogData);
                    this->batteries_.push_back(bat);
                    this->retry_count_ = 0;
                    this->advance_to_next_battery_();
                } else {
                    this->retry_or_skip_("Bad response");
                }
                return;
            }
            // Timeout check
            if (millis() - this->request_start_ > 500) {
                this->retry_or_skip_("Timeout");
            }
            return;
        }
    }
}

void PylonRS485::retry_or_skip_(const char *reason) {
    static const uint8_t MAX_RETRIES = 3;
    if (this->retry_count_ < MAX_RETRIES) {
        this->retry_count_++;
        ESP_LOGW("PylonRS485", "%s for devId 0x%02X, retrying (%d/%d)",
                 reason, this->current_battery_id_, this->retry_count_, MAX_RETRIES);
        this->state_ = State::SEND_REQUEST;
    } else {
        ESP_LOGW("PylonRS485", "%s for devId 0x%02X, giving up after %d retries",
                 reason, this->current_battery_id_, MAX_RETRIES);
        this->retry_count_ = 0;
        this->batteries_.push_back(PylonRS485_Battery{});
        this->advance_to_next_battery_();
    }
}

void PylonRS485::advance_to_next_battery_() {
    if (this->current_battery_id_ < this->battery_count_ + 1) {
        this->current_battery_id_++;
        this->state_ = State::SEND_REQUEST;
    } else {
        // All batteries done — publish
        for (PylontechRS485Listener *listener : this->listeners_) {
            listener->on_line_read(this->batteries_);
        }
        this->state_ = State::IDLE;
    }
}

} // namespace pylon_rs485
} // namespace esphome
