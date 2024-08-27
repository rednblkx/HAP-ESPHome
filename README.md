![logo_2_128](https://github.com/user-attachments/assets/085f5da6-8b3f-4951-90fb-3da5ab52d0dc)

# HAP-ESPHome [![CI](https://github.com/rednblkx/HAP-ESPHome/actions/workflows/main.yml/badge.svg?branch=main)](https://github.com/rednblkx/HAP-ESPHome/actions/workflows/main.yml) [![Discord Badge](https://img.shields.io/badge/Discord-5865F2?logo=discord&logoColor=fff&style=for-the-badge)](https://discordapp.com/invite/VWpZ5YyUcm)

HomeKit support for ESPHome-based ESP32 devices

## 1. Introduction

This project aims to bring HomeKit support to ESP32 devices flashed with an ESPHome configuration that will enable you to directly control the device from the Apple Home app without anything else inbetween.

Components can be imported like any other external compoents as follows:

```yaml
external_components:
  source: github://rednblkx/HAP-ESPHome@main
  refresh: 0s
```

See [Components](#3-components) for documentation.

> [!IMPORTANT]  
> Some components like Bluetooth for example, take up a lot of space in RAM and will result in error during compiling, something like `section '.iram0.text' will not fit in region 'iram0_0_seg'` will be present in the log.

### Supported entity types

| Type   | Attributes                                                            | Notes                                                                                                                                               |
|--------|-----------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| Light  | On/Off, Brightness, RGB, Color Temperature                            |                                                                                                                                                     |
| Lock   | Lock/Unlock                                                           | Homekey can be enabled but only the `pn532_spi` component is supported to be used with it                                                           |
| Switch | On/Off                                                                |                                                                                                                                                     |
| Sensor | Temperature, Humidity, Illuminance, Air Quality, CO2, CO, PM10, PM2.5 | `device_class` property has to be declared with the sensor type as per HASS [docs](https://www.home-assistant.io/integrations/sensor/#device-class) |

## 2. Essentials

The underlying [esp-homekit-sdk](https://github.com/rednblkx/esp-homekit-sdk) library and the components were dedigned to be used with ESP-IDF 5 and HKDF needs to be enabled for mbedtls, see below required configuration

```yaml
esp32:
  board: <insert board id>
  framework:
    type: esp-idf
    version: 5.2.1
    platform_version: 6.7.0
    sdkconfig_options:
      CONFIG_COMPILER_OPTIMIZATION_SIZE: y
      CONFIG_LWIP_MAX_SOCKETS: "16"
      CONFIG_MBEDTLS_HKDF_C: y
```

## 3. Components

Project is divided into two different components, `homekit_base` which handles the bridge logic and `homekit` that handles the actual accessory logic (lights, switches, etc.).

This repository also includes the `pn532` and `pn532_spi` components which are just slightly modified versions of the official ones from the ESPHome repository to suit HomeKey needs with no extra options added to them nor deleted, however, it's not guaranteed to be kept up to date with upstream changes.

### 3.1. `homekit_base`

> [!NOTE]  
> The `homekit_base` component does not have to included in the configuration unless you are interested in one of the properties listed below as it is automatically loaded by the `homekit` component

### Configuration variables:

- **port** (Optional, int): The port HomeKit should listen to
- **meta** (Optional): Bridge information
  - **name** (Optional, string): Name of the bridge accessory
  - **model** (Optional, string): Model name for the bridge accessory
  - **manufacturer** (Optional, string): Manufacturer name for the bridge accessory
  - **serial_number** (Optional, string): Serial number for the bridge accessory
  - **fw_rev** (Optional, string): Firmware revision for the bridge accessory
- **setup_code** (Optional, string): The HomeKit setup code in the format `XXX-XX-XXX` - **Default:** `159-35-728`
- **setup_id** (Optional, string): The Setup ID that can be used to generate a pairing QR Code - **Default:** `ES32`

### Factory reset

It can also be used as a platform component for the button component to reset the HomeKit pairings, see example below:

  ```yaml
  button:
  - platform: homekit_base
    factory_reset:
      name: "Reset HomeKit pairings"
  ```
This will function like any regular button in ESPHome and therefore will be visible in the Web Interface and HASS.

### 3.2. `homekit`

This is what handles the accessory logic like syncing states between HomeKit and ESPHome and basic information (name, attributes, etc.).

> [!TIP]
> For configuration examples, you can see the `.yaml` files in this repository, like [lights-c3.yaml](lights-c3.yaml)

### Configuration variables:
- **light** (Optional): Array of Light entities
  - **id** (Required, [Light](https://esphome.io/components/light/)) - Id of the light entity
  - **meta** (Optional): Accessory information
    - **name** (Optional, string): Name of the accessory, defaults to name of the entity
    - **model** (Optional, string): Model name for the accessory
    - **manufacturer** (Optional, string): Manufacturer name for the accessory
    - **serial_number** (Optional, string): Serial number for the accessory, defaults to internal object id
    - **fw_rev** (Optional, string): Firmware revision for the accessory
- **lock** (Optional): Array of Lock Entities
  - **id** (Required, [Lock](https://esphome.io/components/lock/)) - Id of the lock entity
  - **meta** (Optional): Accessory information
    - **name** (Optional, string): Name of the accessory, defaults to name of the entity
    - **model** (Optional, string): Model name for the accessory
    - **manufacturer** (Optional, string): Manufacturer name for the accessory
    - **serial_number** (Optional, string): Serial number for the accessory, defaults to internal object id
    - **fw_rev** (Optional, string): Firmware revision for the accessory
  - **nfc_id** (Optional, [PN532](https://esphome.io/components/binary_sensor/pn532.html#over-spi)): Id of the `pn532_spi` component, used for the HomeKey functionality
  - **on_hk_success** (Optional, [Action](https://esphome.io/automations/actions)): Action to be executed when Homekey is successfully authenticated
  - **on_hk_fail** (Optional, [Action](https://esphome.io/automations/actions)): Action to be executed when Homekey fails to authenticate
  - **hk_hw_finish**(Optional, string): Color of the Homekey card from the predefined `BLACK`, `SILVER`, `GOLD` and `TAN`, defaults to `BLACK`
- **sensor**
  - **id** (Required, [Sensor](https://esphome.io/components/sensor/)): Id of the sensor entity
  - **meta** (Optional): Accessory information
    - **name** (Optional, string): Name of the accessory, defaults to name of the entity
    - **model** (Optional, string): Model name for the accessory
    - **manufacturer** (Optional, string): Manufacturer name for the accessory
    - **serial_number** (Optional, string): Serial number for the accessory, defaults to internal object id
    - **fw_rev** (Optional, string): Firmware revision for the accessory
- **switch**
  - **id** (Required, [Switch](https://esphome.io/components/switch/)): Id of the switch entity
  - **meta** (Optional): Accessory information
    - **name** (Optional, string): Name of the accessory, defaults to name of the entity
    - **model** (Optional, string): Model name for the accessory
    - **manufacturer** (Optional, string): Manufacturer name for the accessory
    - **serial_number** (Optional, string): Serial number for the accessory, defaults to internal object id
    - **fw_rev** (Optional, string): Firmware revision for the accessory

## 4. HomeKey

If you notice an error in the logs that says `Can't decode message length.`, you can safely ignore it. The project is using a "hijacked" version of the official pn532 component and the message is part of the normal operation of it since it's meant to be used with "regular" NFC Tags.

### 4.1 Disclaimer

The functionality of Homekey is entirely based on reverse engineering since HomeKit specs stopped being available to hobbyists and therefore the entire functionality or parts of it might or might not break in the future and/or lack official features or any internal implementations.

### 4.2 Setup

> [!IMPORTANT]
> Only PN532 over SPI(`pn532_spi` component) is supported at the moment due to required modifications that haven't been ported to other protocols or chips

> [!NOTE]
> For quick reactions and to avoid issues, don't raise the `update_interval` over 500ms

```yaml
spi:
  clk_pin: 4
  miso_pin: 5
  mosi_pin: 6

pn532_spi:
  id: nfc_spi_module
  cs_pin: 7
  update_interval: 100ms

homekit:
  lock:
    - id: <insert lock id>
      nfc_id: nfc_spi_module
```

## Support & Contributing

If you wish to contribute with a feature or a fix, a PR will be most welcomed

The best way to support the work that i do is to :star: the repository but you can also buy me a :coffee: if you wish by sponsoring me on [GitHub](https://github.com/sponsors/rednblkx).

## Credits

[@kormax](https://github.com/kormax) - Homekey NFC protocol [research](https://github.com/kormax/apple-home-key), [ECP](https://github.com/kormax/apple-enhanced-contactless-polling) and [PoC](https://github.com/kormax/apple-home-key-reader)

[@kupa22](https://github.com/kupa22) - [Documenting](https://github.com/kupa22/apple-homekey) the HAP part for the Homekey

## License

This repository consists of multiple licenses since it contains some components([pn532](https://github.com/rednblkx/HAP-ESPHome/tree/main/components/pn532) and [pn532_spi](https://github.com/rednblkx/HAP-ESPHome/tree/main/components/pn532_spi)) originally from the [ESPHome](https://github.com/esphome/esphome) repository, please consult the LICENSE file in each folder in the [components](https://github.com/rednblkx/HAP-ESPHome/tree/main/components) folder.
