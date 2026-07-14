# BlueRiiot Proxy

ESPHome packages and a custom component for reading a BlueRiiot/Blue Connect sensor over BLE, optionally using BlueRiiot API readbacks for calibrated values.

## Required Secrets

Copy `packages/secrets.example.yaml` into your ESPHome secrets location and fill real values.

Required keys:

```yaml
blueriiot_mac: "00:A0:50:00:00:00"
blueriiot_api_pool_id: "00000000-0000-0000-0000-000000000000"
blueriiot_api_blue_device_serial: "00000000"
blueriiot_api_blue_sensor_serial: "00000000000"
blueriiot_username: "user@example.com"
blueriiot_password: "your-password"
```

## Compile

Use Docker from the repository root, not a host ESPHome install:

```bash
docker run --rm -v "$PWD:/config" ghcr.io/esphome/esphome:latest compile /config/blueriiot-proxy.yaml
```

For a repository-local compile without real secrets, mount the example secrets as `/config/secrets.yaml`:

```bash
docker run --rm -v "$PWD:/config" -v "$PWD/packages/secrets.example.yaml:/config/secrets.yaml:ro" ghcr.io/esphome/esphome:latest compile /config/blueriiot-proxy.yaml
```

Expected result:

- Docker pulls the latest ESPHome image when needed.
- ESPHome validates YAML and external component code generation.
- PlatformIO builds the firmware for ESP32-C3 with ESP-IDF.

## BLE Lifecycle

- The BLE tracker remains stopped during boot and starts after the first native API client connects.
- Scanning is passive and continuous while an API client is connected. Passive scanning does not send scan requests to the BlueRiiot sensor.
- Pressing Measure enables the BLE client. The first matching advertisement starts one bounded connection attempt.
- The BLE client is disabled after a valid packet, a disconnect, or a timeout. API work starts only after the BLE client is disabled.
- `${blueriiot_ble_timeout}` limits how long Measure waits for an advertisement. A detected sensor gets a separate 25-second connection and packet deadline; the ESP32 BLE connection timeout is 20 seconds.

`packages/ble.yaml` adds native API connection callbacks. ESPHome merges these callbacks with an existing `api:` configuration.

## Calibration Modes

- `local`: final values use raw BLE values plus Home Assistant offsets.
- `api`: final values use API-calibrated values when API is enabled and a fresh readback exists; otherwise the component falls back to local values and marks status.

The offset sync button derives local offsets from a fresh raw packet and API-calibrated readback unless API verification finds direct coefficient fields.

## Include In An Existing ESPHome Device

Add this to your existing device YAML and adjust values. ESPHome will clone and cache the GitHub repository in `.esphome`.

```yaml
substitutions:
  blueriiot_name_prefix: "BlueRiiot Pool"
  blueriiot_id_prefix: "pool"
  blueriiot_mac: !secret blueriiot_mac
  blueriiot_api_pool_id: !secret blueriiot_api_pool_id
  blueriiot_api_blue_device_serial: !secret blueriiot_api_blue_device_serial
  blueriiot_api_blue_sensor_serial: !secret blueriiot_api_blue_sensor_serial
  blueriiot_username: !secret blueriiot_username
  blueriiot_password: !secret blueriiot_password
  blueriiot_api_readback_delay: 10s
  blueriiot_ble_timeout: 35s

external_components:
  - source: github://whizkid79/esphome_blueriiot_proxy@main
    components: [blueriiot_proxy]
    refresh: 1d

time:
  - platform: homeassistant
    id: homeassistant_time

esphome:
  on_boot:
    priority: -100
    then:
      - wait_until:
          condition:
            api.connected:
              state_subscription_only: true
      - script.execute: script_${blueriiot_id_prefix}_publish_last_good_values

packages:
  blueriiot_entities: github://whizkid79/esphome_blueriiot_proxy/packages/entities.yaml@main
  blueriiot_ble: github://whizkid79/esphome_blueriiot_proxy/packages/ble.yaml@main
  blueriiot_api: github://whizkid79/esphome_blueriiot_proxy/packages/api.yaml@main
```

Do not include `packages/base.yaml` if your device already defines `wifi`, `api`, `ota`, `logger`, and diagnostic basics. `packages/base.yaml` is intended for the standalone `blueriiot-proxy.yaml` file.

If your existing YAML already has a `time:` component with id `homeassistant_time`, do not add a second one.

The API package uses ESP-IDF's default root certificate bundle via `verify_ssl: true`. No static BlueRiiot CA certificate is bundled.

## Last Compile Verification

- Date: 2026-07-14
- Command: `docker run --rm -v "$PWD:/config" -v "$PWD/packages/secrets.example.yaml:/config/secrets.yaml:ro" ghcr.io/esphome/esphome:latest compile /config/blueriiot-proxy.yaml`
- Result: `PASS`
- ESPHome version from output: `2026.6.5`
- ESP-IDF version from output: `5.5.4`
