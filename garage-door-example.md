# HAP Garage Door Example

This example demonstrates how to set up a garage door using ESPHome cover component. HomeKit cover integration will be available once cover support is added to the upstream HAP-ESPHome repository.

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

external_components:
  source: github://rednblkx/HAP-ESPHome@main
  refresh: 0s

# Future HomeKit base configuration (uncomment when cover support is added)
# homekit_base:
#   setup_code: '159-35-728'

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
      - output.turn_off: garage_close_relay
      - output.turn_on: garage_open_relay
      - delay: 1s
      - output.turn_off: garage_open_relay
      
    close_action:
      - logger.log: "Closing garage door"
      - output.turn_off: garage_open_relay
      - output.turn_on: garage_close_relay
      - delay: 1s
      - output.turn_off: garage_close_relay
      
    stop_action:
      - logger.log: "Stopping garage door"
      - output.turn_off: garage_open_relay
      - output.turn_off: garage_close_relay

# Future HomeKit cover configuration (uncomment when cover support is added)
# homekit:
#   cover:
#     - id: garage_door
#       meta:
#         name: "Main Garage Door"
#         manufacturer: "ESPHome"
#         model: "Smart Garage Controller"
#         serial_number: "GD123456"
#         fw_rev: "1.0.0"

logger:
  level: DEBUG
```

## Features

- Standard ESPHome garage door control (ready for HomeKit when upstream support is added)
- Template cover configured as garage door
- GPIO relay control for opening/closing
- Position sensors for detecting door state  
- Logger debugging support

## Hardware Requirements

- ESP32 development board
- 2 relays for motor control (open/close)
- 2 limit switches or sensors for position detection
- Appropriate power supply and wiring

## State Mapping

| Sensor State | Cover State |
|--------------|-------------|
| garage_open_sensor = ON | COVER_OPEN |
| garage_close_sensor = ON | COVER_CLOSED |
| Both sensors = OFF | Unknown/Intermediate |

## Future HomeKit Integration

This configuration is ready for HomeKit integration once cover support is added to the upstream HAP-ESPHome repository. The commented HomeKit configuration sections can be uncommented when support becomes available.

The planned HomeKit integration will provide:
- Garage Door Opener service in Apple Home app
- Proper state synchronization between ESPHome and HomeKit
- Support for Open/Close commands from HomeKit

**Note:** This example currently uses the upstream `rednblkx/HAP-ESPHome` repository which does not yet include cover support for HomeKit integration.