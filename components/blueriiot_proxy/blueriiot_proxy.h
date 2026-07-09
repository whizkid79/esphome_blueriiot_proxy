#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "blueriiot_api.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace blueriiot_proxy {

enum class CalibrationSource : uint8_t {
  LOCAL,
  API,
};

struct RawPacket {
  uint8_t frame = 0;
  uint16_t temperature_raw = 0;
  uint16_t ph_raw = 0;
  uint16_t orp_raw = 0;
  uint16_t conductivity_raw = 0;
  uint16_t battery_raw_mv = 0;
  uint8_t extra = 0;
  std::string hex;
};

struct Values {
  float temperature = NAN;
  float ph = NAN;
  float orp = NAN;
  float battery = NAN;
};

class BlueRiiotProxy : public Component {
 public:
  void setup() override;
  void dump_config() override;

  bool handle_packet(const std::string &packet);
  void set_temperature_offset(float value);
  void set_ph_offset(float value);
  void set_orp_offset(float value);
  void set_api_enabled(bool enabled);
  void set_calibration_source(const std::string &source);
  void set_api_values(float temperature, float ph, float orp);
  void apply_api_derived_offsets(float temperature, float ph, float orp);
  void publish_cached_final_values(float temperature, float ph, float orp, float battery);
  void publish_status(const std::string &status);
  bool has_fresh_packet(uint32_t max_age_ms) const;
  bool has_final_values() const;
  float final_temperature() const;
  float final_ph() const;
  float final_orp() const;
  float final_battery() const;
  float derive_temperature_offset(float api_temperature) const;
  float derive_ph_offset(float api_ph) const;
  float derive_orp_offset(float api_orp) const;
  std::string raw_hex() const;

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor = sensor; }
  void set_ph_sensor(sensor::Sensor *sensor) { this->ph_sensor = sensor; }
  void set_orp_sensor(sensor::Sensor *sensor) { this->orp_sensor = sensor; }
  void set_temperature_local_sensor(sensor::Sensor *sensor) { this->temperature_local_sensor = sensor; }
  void set_ph_local_sensor(sensor::Sensor *sensor) { this->ph_local_sensor = sensor; }
  void set_orp_local_sensor(sensor::Sensor *sensor) { this->orp_local_sensor = sensor; }
  void set_temperature_api_sensor(sensor::Sensor *sensor) { this->temperature_api_sensor = sensor; }
  void set_ph_api_sensor(sensor::Sensor *sensor) { this->ph_api_sensor = sensor; }
  void set_orp_api_sensor(sensor::Sensor *sensor) { this->orp_api_sensor = sensor; }
  void set_battery_sensor(sensor::Sensor *sensor) { this->battery_sensor = sensor; }
  void set_raw_length_sensor(sensor::Sensor *sensor) { this->raw_length_sensor = sensor; }
  void set_raw_frame_sensor(sensor::Sensor *sensor) { this->raw_frame_sensor = sensor; }
  void set_raw_temperature_sensor(sensor::Sensor *sensor) { this->raw_temperature_sensor = sensor; }
  void set_raw_ph_sensor(sensor::Sensor *sensor) { this->raw_ph_sensor = sensor; }
  void set_raw_orp_sensor(sensor::Sensor *sensor) { this->raw_orp_sensor = sensor; }
  void set_raw_conductivity_sensor(sensor::Sensor *sensor) { this->raw_conductivity_sensor = sensor; }
  void set_raw_battery_mv_sensor(sensor::Sensor *sensor) { this->raw_battery_mv_sensor = sensor; }
  void set_raw_extra_sensor(sensor::Sensor *sensor) { this->raw_extra_sensor = sensor; }

  void set_raw_hex_text_sensor(text_sensor::TextSensor *sensor) { this->raw_hex_text_sensor = sensor; }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { this->status_text_sensor = sensor; }
  void set_last_error_text_sensor(text_sensor::TextSensor *sensor) { this->last_error_text_sensor = sensor; }
  void set_ph_status_text_sensor(text_sensor::TextSensor *sensor) { this->ph_status_text_sensor = sensor; }
  void set_orp_status_text_sensor(text_sensor::TextSensor *sensor) { this->orp_status_text_sensor = sensor; }

  void set_raw_packet_valid_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->raw_packet_valid_binary_sensor = sensor;
  }

 protected:
  bool parse_packet_(const std::string &packet, RawPacket *raw);
  Values calculate_local_(const RawPacket &raw) const;
  Values calculate_base_(const RawPacket &raw) const;
  void publish_raw_(const RawPacket &raw);
  void publish_local_(const Values &values);
  void publish_final_();
  void publish_quality_(const Values &values);
  bool is_error_status_(const std::string &status) const;

  sensor::Sensor *temperature_sensor{nullptr};
  sensor::Sensor *ph_sensor{nullptr};
  sensor::Sensor *orp_sensor{nullptr};
  sensor::Sensor *temperature_local_sensor{nullptr};
  sensor::Sensor *ph_local_sensor{nullptr};
  sensor::Sensor *orp_local_sensor{nullptr};
  sensor::Sensor *temperature_api_sensor{nullptr};
  sensor::Sensor *ph_api_sensor{nullptr};
  sensor::Sensor *orp_api_sensor{nullptr};
  sensor::Sensor *battery_sensor{nullptr};
  sensor::Sensor *raw_length_sensor{nullptr};
  sensor::Sensor *raw_frame_sensor{nullptr};
  sensor::Sensor *raw_temperature_sensor{nullptr};
  sensor::Sensor *raw_ph_sensor{nullptr};
  sensor::Sensor *raw_orp_sensor{nullptr};
  sensor::Sensor *raw_conductivity_sensor{nullptr};
  sensor::Sensor *raw_battery_mv_sensor{nullptr};
  sensor::Sensor *raw_extra_sensor{nullptr};

  text_sensor::TextSensor *raw_hex_text_sensor{nullptr};
  text_sensor::TextSensor *status_text_sensor{nullptr};
  text_sensor::TextSensor *last_error_text_sensor{nullptr};
  text_sensor::TextSensor *ph_status_text_sensor{nullptr};
  text_sensor::TextSensor *orp_status_text_sensor{nullptr};

  binary_sensor::BinarySensor *raw_packet_valid_binary_sensor{nullptr};

  RawPacket last_raw_{};
  Values local_values_{};
  Values api_values_{};
  Values final_values_{};
  uint32_t last_packet_ms_{0};
  float temperature_offset_{0.0f};
  float ph_offset_{0.0f};
  float orp_offset_{0.0f};
  bool api_enabled_{false};
  bool api_values_valid_{false};
  bool final_values_valid_{false};
  bool has_raw_packet_{false};
  CalibrationSource calibration_source_{CalibrationSource::LOCAL};
};

}  // namespace blueriiot_proxy
}  // namespace esphome
