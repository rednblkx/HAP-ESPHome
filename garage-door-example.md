# HAP Garage Door Example

This example demonstrates how to set up a garage door using the HAP-ESPHome cover component.

```yaml
esphome:
  name: garage-door-controller

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  reboot_timeout: 0s

esp32:
  board: esp32dev
  framework:
    type: esp-idf

# Example relay outputs for controlling garage door motor
output:
  - platform: gpio
    pin: GPIO12
    id: garage_open_relay
    
  - platform: gpio
    pin: GPIO13
    id: garage_close_relay

# Example sensors for detecting garage door position
binary_sensor:
  - platform: gpio
    pin: 
      number: GPIO14
      mode: INPUT_PULLUP
      inverted: true
    id: garage_open_sensor
    name: "Garage Door Open Sensor"
    
  - platform: gpio
    pin: 
      number: GPIO15
      mode: INPUT_PULLUP
      inverted: true
    id: garage_close_sensor
    name: "Garage Door Close Sensor"

# Template cover that maps to garage door control
cover:
  - platform: template
    name: "Garage Door"
    id: garage_door
    device_class: garage
    optimistic: false
    assumed_state: false
    
    # Current position based on sensors
    lambda: |-
      if (id(garage_open_sensor).state) {
        return COVER_OPEN;
      } else if (id(garage_close_sensor).state) {
        return COVER_CLOSED;
      } else {
        return {};  // Unknown state
      }
    
    open_action:
      - logger.log: "Opening garage door"
      - output.turn_on: garage_open_relay
      - delay: 1s
      - output.turn_off: garage_open_relay
      
    close_action:
      - logger.log: "Closing garage door"
      - output.turn_on: garage_close_relay
      - delay: 1s
      - output.turn_off: garage_close_relay
      
    stop_action:
      - logger.log: "Stopping garage door"
      - output.turn_off: garage_open_relay
      - output.turn_off: garage_close_relay

external_components:
  source: github://donavanbecker/HAP-ESPHome@copilot/fix-1
  refresh: 0s

homekit_base:
  setup_code: '159-35-728'

# HomeKit integration - exposes cover as garage door opener
homekit:
  cover:
    - id: garage_door
      meta:
        name: "Main Garage Door"
        manufacturer: "ESPHome"
        model: "Smart Garage Controller"
        serial_number: "GD123456"
        fw_rev: "1.0.0"

logger:
  level: DEBUG
```

## Features

- Exposes ESPHome cover as HomeKit Garage Door Opener
- Maps cover states to proper garage door states in HomeKit
- Supports open, close, and stop operations
- Shows current door state (open/closed/opening/closing/stopped)
- Integrates with Home app and Siri voice control

## State Mapping

| ESPHome State | HomeKit State |
|---------------|---------------|
| COVER_OPERATION_IDLE + position=1.0 | Open (0) |
| COVER_OPERATION_IDLE + position=0.0 | Closed (1) |
| COVER_OPERATION_OPENING | Opening (2) |
| COVER_OPERATION_CLOSING | Closing (3) |
| COVER_OPERATION_IDLE + partial position | Stopped (4) |