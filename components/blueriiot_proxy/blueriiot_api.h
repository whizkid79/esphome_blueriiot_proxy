#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/version.h"

namespace blueriiot_api {

inline std::string bytes_to_hex(const unsigned char *data, size_t size) {
  static const char *digits = "0123456789abcdef";
  std::string result;
  result.reserve(size * 2);
  for (size_t i = 0; i < size; i++) {
    result.push_back(digits[(data[i] >> 4) & 0x0F]);
    result.push_back(digits[data[i] & 0x0F]);
  }
  return result;
}

inline std::string sha256_hex(const std::string &value) {
  unsigned char output[32];
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_sha256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), output, 0);
#else
  mbedtls_sha256_ret(reinterpret_cast<const unsigned char *>(value.data()), value.size(), output, 0);
#endif
  return bytes_to_hex(output, sizeof(output));
}

inline std::vector<unsigned char> hmac_sha256(
    const std::vector<unsigned char> &key,
    const std::string &message) {
  std::vector<unsigned char> output(32);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(
      info,
      key.data(),
      key.size(),
      reinterpret_cast<const unsigned char *>(message.data()),
      message.size(),
      output.data());
  return output;
}

inline std::vector<unsigned char> hmac_sha256(const std::string &key, const std::string &message) {
  return hmac_sha256(std::vector<unsigned char>(key.begin(), key.end()), message);
}

inline std::string trim_and_compress_spaces(const std::string &value) {
  std::string result;
  bool pending_space = false;
  bool has_content = false;

  for (char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      pending_space = has_content;
      continue;
    }
    if (pending_space && !result.empty()) {
      result.push_back(' ');
    }
    result.push_back(ch);
    pending_space = false;
    has_content = true;
  }

  return result;
}

inline bool is_uri_unreserved(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.' ||
         ch == '~';
}

inline std::string canonical_uri(const std::string &path) {
  static const char *digits = "0123456789ABCDEF";
  std::string source = path.empty() ? std::string("/") : path;
  std::string result;

  for (unsigned char ch : source) {
    if (ch == '/' || is_uri_unreserved(static_cast<char>(ch))) {
      result.push_back(static_cast<char>(ch));
      continue;
    }
    result.push_back('%');
    result.push_back(digits[(ch >> 4) & 0x0F]);
    result.push_back(digits[ch & 0x0F]);
  }

  if (result.empty() || result[0] != '/') {
    result.insert(result.begin(), '/');
  }
  return result;
}

inline std::string signing_key_hex(
    const std::string &secret_key,
    const std::string &date_stamp,
    const std::string &region,
    const std::string &service,
    const std::string &string_to_sign) {
  auto key_date = hmac_sha256("AWS4" + secret_key, date_stamp);
  auto key_region = hmac_sha256(key_date, region);
  auto key_service = hmac_sha256(key_region, service);
  auto key_signing = hmac_sha256(key_service, "aws4_request");
  auto signature = hmac_sha256(key_signing, string_to_sign);
  return bytes_to_hex(signature.data(), signature.size());
}

inline std::string authorization_header(
    const std::string &method,
    const std::string &path,
    const std::string &query,
    const std::string &body,
    const std::string &access_key,
    const std::string &secret_key,
    const std::string &session_token,
    const std::string &amz_date,
    const std::string &date_stamp,
    const std::string &host,
    const std::string &user_agent,
    const std::string &accept_language) {
  constexpr const char *region = "eu-west-1";
  constexpr const char *service = "execute-api";

  std::map<std::string, std::string> headers = {
      {"accept", "*/*"},
      {"accept-language", accept_language},
      {"content-type", "application/json"},
      {"host", host},
      {"user-agent", user_agent},
      {"x-amz-date", amz_date},
      {"x-amz-security-token", session_token},
  };

  std::string canonical_headers;
  std::string signed_headers;
  for (const auto &header : headers) {
    canonical_headers += header.first + ":" + trim_and_compress_spaces(header.second) + "\n";
    if (!signed_headers.empty()) {
      signed_headers += ";";
    }
    signed_headers += header.first;
  }

  const std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
  const std::string canonical_request = method + "\n" + canonical_uri(path) + "\n" + query + "\n" +
                                        canonical_headers + "\n" + signed_headers + "\n" +
                                        sha256_hex(body);
  const std::string string_to_sign = "AWS4-HMAC-SHA256\n" + amz_date + "\n" + credential_scope +
                                     "\n" + sha256_hex(canonical_request);

  return "AWS4-HMAC-SHA256 Credential=" + access_key + "/" + credential_scope +
         ", SignedHeaders=" + signed_headers +
         ", Signature=" + signing_key_hex(secret_key, date_stamp, region, service, string_to_sign);
}

inline std::string authorization_header(
    const std::string &method,
    const std::string &path,
    const std::string &body,
    const std::string &access_key,
    const std::string &secret_key,
    const std::string &session_token,
    const std::string &amz_date,
    const std::string &date_stamp,
    const std::string &host,
    const std::string &user_agent,
    const std::string &accept_language) {
  return authorization_header(
      method,
      path,
      "",
      body,
      access_key,
      secret_key,
      session_token,
      amz_date,
      date_stamp,
      host,
      user_agent,
      accept_language);
}

inline std::string json_escape(const std::string &value) {
  std::string result;
  result.reserve(value.size() + 8);

  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        result += "\\\\";
        break;
      case '"':
        result += "\\\"";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        if (ch < 0x20) {
          char buffer[7];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
          result += buffer;
        } else {
          result.push_back(static_cast<char>(ch));
        }
        break;
    }
  }

  return result;
}

inline std::string raw_measure_body(
    const std::string &blue_device_serial,
    const std::string &blue_sensor_serial,
    const std::string &version,
    const std::string &created,
    const std::string &raw_hex) {
  std::string measure = "{\"blue_device_serial\":\"" + json_escape(blue_device_serial) + "\"";

  if (!blue_sensor_serial.empty()) {
    measure += ",\"blue_sensor_serial\":\"" + json_escape(blue_sensor_serial) + "\"";
  }

  measure += ",\"version\":\"" + json_escape(version) + "\"" + ",\"created\":\"" +
             json_escape(created) + "\"" + ",\"data\":\"" + json_escape(raw_hex) + "\"}";

  return "{\"data\":[" + measure + "],\"count\":1}";
}

}  // namespace blueriiot_api
