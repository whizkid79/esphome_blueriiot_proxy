#include "blueriiot_proxy.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace blueriiot_proxy {

static const char *const TAG = "blueriiot";

static std::string classify_ph(float ph) {
  if (std::isnan(ph)) return "UNKNOWN";
  if (ph < 7.1f) return "LOW";
  if (ph <= 7.5f) return "GOOD";
  return "HIGH";
}

static std::string classify_orp(float orp) {
  if (std::isnan(orp)) return "UNKNOWN";
  if (orp < 650.0f) return "LOW";
  if (orp <= 900.0f) return "GOOD";
  return "HIGH";
}

void BlueRiiotProxy::setup() {
  ESP_LOGI("blueriiot_state", "Booting BlueRiiot component");
  this->publish_status("IDLE");
}

void BlueRiiotProxy::dump_config() {
  ESP_LOGCONFIG(TAG, "BlueRiiot Proxy");
}

bool BlueRiiotProxy::handle_packet(const std::string &packet) {
  RawPacket raw;
  if (!this->parse_packet_(packet, &raw)) {
    this->publish_status("BLE_PACKET_INVALID");
    if (this->raw_packet_valid_binary_sensor != nullptr) {
      this->raw_packet_valid_binary_sensor->publish_state(false);
    }
    return false;
  }

  this->last_raw_ = raw;
  this->has_raw_packet_ = true;
  this->last_packet_ms_ = millis();
  this->local_values_ = this->calculate_local_(raw);
  this->publish_raw_(raw);
  this->publish_local_(this->local_values_);
  this->publish_final_();
  this->publish_status("BLE_PACKET_OK");
  if (this->raw_packet_valid_binary_sensor != nullptr) {
    this->raw_packet_valid_binary_sensor->publish_state(true);
  }
  return true;
}

void BlueRiiotProxy::set_temperature_offset(float value) {
  this->temperature_offset_ = value;
  if (!this->has_raw_packet_) return;
  this->local_values_ = this->calculate_local_(this->last_raw_);
  this->publish_local_(this->local_values_);
  this->publish_final_();
}

void BlueRiiotProxy::set_ph_offset(float value) {
  this->ph_offset_ = value;
  if (!this->has_raw_packet_) return;
  this->local_values_ = this->calculate_local_(this->last_raw_);
  this->publish_local_(this->local_values_);
  this->publish_final_();
}

void BlueRiiotProxy::set_orp_offset(float value) {
  this->orp_offset_ = value;
  if (!this->has_raw_packet_) return;
  this->local_values_ = this->calculate_local_(this->last_raw_);
  this->publish_local_(this->local_values_);
  this->publish_final_();
}

void BlueRiiotProxy::set_api_enabled(bool enabled) {
  this->api_enabled_ = enabled;
  this->publish_final_();
}

void BlueRiiotProxy::set_calibration_source(const std::string &source) {
  this->calibration_source_ = source == "api" ? CalibrationSource::API : CalibrationSource::LOCAL;
  this->publish_final_();
}

void BlueRiiotProxy::set_api_values(float temperature, float ph, float orp) {
  this->api_values_ = {temperature, ph, orp, this->local_values_.battery};
  this->api_values_valid_ = true;
  if (this->temperature_api_sensor != nullptr) this->temperature_api_sensor->publish_state(temperature);
  if (this->ph_api_sensor != nullptr) this->ph_api_sensor->publish_state(ph);
  if (this->orp_api_sensor != nullptr) this->orp_api_sensor->publish_state(orp);
  this->publish_final_();
}

void BlueRiiotProxy::apply_api_derived_offsets(float temperature, float ph, float orp) {
  if (!this->has_fresh_packet(120000)) {
    this->publish_status("OFFSET_SYNC_NO_FRESH_PACKET");
    return;
  }
  this->temperature_offset_ = this->derive_temperature_offset(temperature);
  this->ph_offset_ = this->derive_ph_offset(ph);
  this->orp_offset_ = this->derive_orp_offset(orp);
  this->local_values_ = this->calculate_local_(this->last_raw_);
  this->publish_local_(this->local_values_);
  this->publish_final_();
}

void BlueRiiotProxy::publish_cached_final_values(float temperature, float ph, float orp, float battery) {
  if (!std::isfinite(temperature) || !std::isfinite(ph) || !std::isfinite(orp)) {
    this->publish_status("CACHE_INVALID");
    return;
  }
  this->final_values_ = {temperature, ph, orp, battery};
  this->final_values_valid_ = true;
  if (this->temperature_sensor != nullptr) this->temperature_sensor->publish_state(temperature);
  if (this->ph_sensor != nullptr) this->ph_sensor->publish_state(ph);
  if (this->orp_sensor != nullptr) this->orp_sensor->publish_state(orp);
  if (std::isfinite(battery) && this->battery_sensor != nullptr) this->battery_sensor->publish_state(battery);
  this->publish_quality_(this->final_values_);
  this->publish_status("CACHE_REPUBLISHED");
}

void BlueRiiotProxy::publish_status(const std::string &status) {
  const uint32_t age = this->last_packet_ms_ == 0 ? 0 : millis() - this->last_packet_ms_;
  ESP_LOGI(
      "blueriiot_state",
      "state=%s raw_age_ms=%u api_enabled=%s",
      status.c_str(),
      age,
      this->api_enabled_ ? "true" : "false");
  if (this->status_text_sensor != nullptr) {
    this->status_text_sensor->publish_state(status);
  }
  if (this->last_error_text_sensor != nullptr && this->is_error_status_(status)) {
    this->last_error_text_sensor->publish_state(status);
  }
}

bool BlueRiiotProxy::has_fresh_packet(uint32_t max_age_ms) const {
  return this->last_packet_ms_ != 0 && millis() - this->last_packet_ms_ <= max_age_ms;
}

bool BlueRiiotProxy::has_final_values() const {
  return this->final_values_valid_;
}

float BlueRiiotProxy::final_temperature() const {
  return this->final_values_.temperature;
}

float BlueRiiotProxy::final_ph() const {
  return this->final_values_.ph;
}

float BlueRiiotProxy::final_orp() const {
  return this->final_values_.orp;
}

float BlueRiiotProxy::final_battery() const {
  return this->final_values_.battery;
}

float BlueRiiotProxy::derive_temperature_offset(float api_temperature) const {
  return api_temperature - this->last_raw_.temperature_raw / 100.0f;
}

float BlueRiiotProxy::derive_ph_offset(float api_ph) const {
  Values base = this->calculate_base_(this->last_raw_);
  return api_ph - base.ph;
}

float BlueRiiotProxy::derive_orp_offset(float api_orp) const {
  return api_orp - this->last_raw_.orp_raw / 4.0f;
}

std::string BlueRiiotProxy::raw_hex() const {
  return this->last_raw_.hex;
}

bool BlueRiiotProxy::parse_packet_(const std::string &packet, RawPacket *raw) {
  if (packet.size() < 11) {
    ESP_LOGW(TAG, "Unexpected packet length: %u", static_cast<unsigned>(packet.size()));
    return false;
  }
  auto u8 = [&](size_t index) -> uint16_t { return static_cast<uint8_t>(packet[index]); };
  auto u16le = [&](size_t index) -> uint16_t { return u8(index) | (u8(index + 1) << 8); };

  static const char *digits = "0123456789ABCDEF";
  raw->hex.clear();
  raw->hex.reserve(packet.size() * 2);
  for (unsigned char byte : packet) {
    raw->hex.push_back(digits[(byte >> 4) & 0x0F]);
    raw->hex.push_back(digits[byte & 0x0F]);
  }

  raw->frame = u8(0);
  raw->temperature_raw = u16le(1);
  raw->ph_raw = u16le(3);
  raw->orp_raw = u16le(5);
  raw->conductivity_raw = u16le(7);
  raw->battery_raw_mv = u16le(9);
  raw->extra = packet.size() > 11 ? u8(11) : 0;
  return true;
}

Values BlueRiiotProxy::calculate_base_(const RawPacket &raw) const {
  Values values;
  values.temperature = raw.temperature_raw / 100.0f;
  values.ph = std::round((7.0f + (2048.0f - raw.ph_raw) / 240.0f) * 10.0f) / 10.0f;
  values.orp = raw.orp_raw / 4.0f;
  values.battery = std::max(0.0f, std::min(100.0f, (raw.battery_raw_mv - 3400.0f) / 240.0f * 100.0f));
  return values;
}

Values BlueRiiotProxy::calculate_local_(const RawPacket &raw) const {
  Values values = this->calculate_base_(raw);
  values.temperature += this->temperature_offset_;
  values.ph += this->ph_offset_;
  values.orp += this->orp_offset_;
  return values;
}

void BlueRiiotProxy::publish_raw_(const RawPacket &raw) {
  if (this->raw_hex_text_sensor != nullptr) this->raw_hex_text_sensor->publish_state(raw.hex);
  if (this->raw_length_sensor != nullptr) this->raw_length_sensor->publish_state(raw.hex.size() / 2);
  if (this->raw_frame_sensor != nullptr) this->raw_frame_sensor->publish_state(raw.frame);
  if (this->raw_temperature_sensor != nullptr) this->raw_temperature_sensor->publish_state(raw.temperature_raw);
  if (this->raw_ph_sensor != nullptr) this->raw_ph_sensor->publish_state(raw.ph_raw);
  if (this->raw_orp_sensor != nullptr) this->raw_orp_sensor->publish_state(raw.orp_raw);
  if (this->raw_conductivity_sensor != nullptr) this->raw_conductivity_sensor->publish_state(raw.conductivity_raw);
  if (this->raw_battery_mv_sensor != nullptr) this->raw_battery_mv_sensor->publish_state(raw.battery_raw_mv);
  if (this->raw_extra_sensor != nullptr) this->raw_extra_sensor->publish_state(raw.extra);
}

void BlueRiiotProxy::publish_local_(const Values &values) {
  if (this->temperature_local_sensor != nullptr) this->temperature_local_sensor->publish_state(values.temperature);
  if (this->ph_local_sensor != nullptr) this->ph_local_sensor->publish_state(values.ph);
  if (this->orp_local_sensor != nullptr) this->orp_local_sensor->publish_state(values.orp);
  if (this->battery_sensor != nullptr) this->battery_sensor->publish_state(values.battery);
}

void BlueRiiotProxy::publish_final_() {
  const bool use_api = this->api_enabled_ && this->api_values_valid_ && this->calibration_source_ == CalibrationSource::API;
  const Values &values = use_api ? this->api_values_ : this->local_values_;
  if (!std::isfinite(values.temperature) || !std::isfinite(values.ph) || !std::isfinite(values.orp)) {
    return;
  }
  this->final_values_ = values;
  this->final_values_valid_ = true;
  if (this->temperature_sensor != nullptr) this->temperature_sensor->publish_state(values.temperature);
  if (this->ph_sensor != nullptr) this->ph_sensor->publish_state(values.ph);
  if (this->orp_sensor != nullptr) this->orp_sensor->publish_state(values.orp);
  this->publish_quality_(values);
}

void BlueRiiotProxy::publish_quality_(const Values &values) {
  if (this->ph_status_text_sensor != nullptr) {
    this->ph_status_text_sensor->publish_state(classify_ph(values.ph));
  }
  if (this->orp_status_text_sensor != nullptr) {
    this->orp_status_text_sensor->publish_state(classify_orp(values.orp));
  }
}

bool BlueRiiotProxy::is_error_status_(const std::string &status) const {
  return status.find("ERROR") != std::string::npos || status.find("INVALID") != std::string::npos ||
         status.find("TIMEOUT") != std::string::npos || status.find("NO_") == 0 ||
         status == "CACHE_INVALID" || status == "API_DISABLED_OR_NO_FRESH_PACKET";
}

}  // namespace blueriiot_proxy
}  // namespace esphome
