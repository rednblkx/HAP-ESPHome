# HAP Garage Door Example

This example demonstrates how to set up a garage door using ESPHome cover component with full HomeKit integration.

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
    version: 5.3.1
    platform_version: 6.8.1

external_components:
  source: github://rednblkx/HAP-ESPHome@main
  refresh: 0s

homekit_base:
  setup_code: '159-35-728'

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

# HomeKit integration
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

- Full HomeKit garage door opener integration
- Template cover configured as garage door
- GPIO relay control for opening/closing
- Position sensors for detecting door state  
- Logger debugging support
- Obstruction detection support

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

## HomeKit Integration

This configuration provides full HomeKit integration for garage door control. The garage door will appear in the Apple Home app as a "Garage Door Opener" with the following features:

- **Open/Close Control**: Tap to open or close the garage door from the Home app
- **Current State Display**: Shows whether the door is open, closed, opening, closing, or stopped
- **Obstruction Detection**: Automatically detects if the door stops in an intermediate position
- **Proper State Synchronization**: Changes made via HomeKit are reflected in ESPHome and vice versa

### HomeKit States Mapping

The integration maps ESPHome cover states to HomeKit garage door states as follows:

| ESPHome State | HomeKit State | Description |
|---------------|---------------|-------------|
| COVER_OPERATION_IDLE + position ≥ 98% | Open (0) | Door is fully open |
| COVER_OPERATION_IDLE + position ≤ 2% | Closed (1) | Door is fully closed |
| COVER_OPERATION_IDLE + intermediate position | Stopped (4) | Door stopped mid-way (potential obstruction) |
| COVER_OPERATION_OPENING | Opening (2) | Door is currently opening |
| COVER_OPERATION_CLOSING | Closing (3) | Door is currently closing |

### Setup in Apple Home App

1. After flashing the configuration, the device will appear as discoverable in HomeKit
2. Use the setup code `159-35-728` (or scan the QR code if displayed)
3. The garage door will appear as "Main Garage Door" (or the name you specified)
4. You can control it via the Home app, Siri, or automation scenes