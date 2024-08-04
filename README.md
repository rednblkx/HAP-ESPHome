# HAP-ESPHome [![Discord Badge](https://img.shields.io/badge/Discord-5865F2?logo=discord&logoColor=fff&style=for-the-badge)](https://discordapp.com/invite/VWpZ5YyUcm)

Accessibility through simplicity

## 1. Introduction

**First of all**, the code could be much better than it is, but it mostly gets the job done from my testing, still, beware of bugs.

This project started as a desire to expand the possibilities of [HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32) by combining it with the powers of a environment like ESPHome that's infinitely configurable and most of the focus was put into porting the functionality of the aforementioned repository.

Now, it would've been a whole lot of wasted potential for this kind of implementation if i were to just link a Lock from ESPHome to HomeKit, and so this project actually implements a couple of entity types from ESPHome that you can link to HomeKit, more on that below.

Components can be imported with the two lines below added to your yaml file, keep reading for documentation on how they can be used.

```yaml
external_components:
  source: github://rednblkx/HAP-ESPHome@main
```

## 2. Entity Types

At the moment, there are only a couple of object types that you can add to HomeKit through this project.

Below you can find a table that represents what types can be linked to HomeKit and which of their attributes are being synced with it.

| Type   | Attributes                                                            | Options                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | Notes                                                                                                                     |   |   |
|--------|-----------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|---|---|
| Light  | On/Off, Brightness, RGB, Color Temperature                            | `id` - ID of the `light:` component that will be linked                                                                                                                                                                                                                                                                                                                                                                                                                 |                                                                                                                           |   |   |
| Lock   | Lock/Unlock                                                           | `id` - ID of the `lock:` component that will be linked<br><br>`nfc_id` - ID of the PN532 component for enabling HomeKey functionality<br>`on_hk_success` - ESPHome Trigger where you can define what should happen when HomeKey is authenticated and two parameters are passed on, the issuer id(account) as `x` and the endpoint id(device) as `y` that can be used within a lambda, those can be used too identify who/what was authenticated<br>`on_hk_fail` - ESPHome Trigger where you can define what should happen when HomeKey fails<br>`hk_hw_finish` - Property that sets the Color of the HomeKey Card from the pre-defined list (SILVER, GOLD, BLACK, TAN) | Modifications were needed both on the frontend and backend and only the SPI driver(pn532_spi component) has been modified |   |   |
| Switch | On/Off                                                                | `id` - ID of the `switch:` component that will be linked                                                                                                                                                                                                                                                                                                                                                                                                                |                                                                                                                           |   |   |
| Sensor | Temperature, Humidity, Illuminance, Air Quality, CO2, CO, PM10, PM2.5 | `id` - ID of the `sensor:` component that will be linked                                                                                                                                                                                                                                                                                                                                                                                                                | `device_class` property has to be declared for each sensor on the yaml file                                               |   |   |


## 3. Essentials

**Important Note:** The underlying [esp-homekit-sdk](https://github.com/rednblkx/esp-homekit-sdk) library and the components were dedigned to be used with ESP-IDF 5, see below required configuration

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

Now that we've established some basics, let's get into the actual stuff.

The HomeKit implementation was configured to act as a bridge in order to have multiple different accessories added and the project itself comprises of two components for this reason(though realistically could've been just one):

- `homekit_base` -> Handles the bridge stuff
  - A `button:` can be assigned as a factory reset button

    ```yaml
    button:
    - platform: homekit_base
      factory_reset:
        name: "Reset Homekit pairings"
    ```

  - Adding property `setup_code` assigns the setup code used during HomeKit pairing

    ```yaml
    homekit_base:
      setup_code: '159-35-728'
    ```

- `homekit` -> Handles the accessories, here is where you actually assign all the entities
  - To add an entity, add the type name as a property (e.g. `light:`) and under it add the property `- id:` and assign to it the id of the entity that you wish to link as per the supported types in the table above.

    ```yaml
    light:
      - platform: binary
        id: desk_light
        name: "Desk Lamp"
        output: simple_led
        restore_mode: RESTORE_DEFAULT_OFF
    homekit:
      light:
        - id: desk_light
    ```

    Or for the more advanced lock component, where you can assign actions to HomeKey triggers

    1.

    ```yaml
    lock:
      - platform: template
        id: "this_lock"
        name: "Main Lock"
        optimistic: True
        on_lock:
        - logger.log: "Door Locked!"
        on_unlock:
        - logger.log: "Door Unlocked!"
    homekit:
      lock:
        - id: this_lock
          nfc_id: nfc_spi_module
          on_hk_success:
            lambda: |-
              ESP_LOGI("TEST", "IssuerID: %s", x.c_str());
              ESP_LOGI("TEST", "EndpointID: %s", y.c_str());
              id(test_light).toggle().perform();
          on_hk_fail:
            lambda: |-
              ESP_LOGI("TEST", "IT FAILED :(");
          hk_hw_finish: "SILVER"
    ```

    2.

    ```yaml
    lock:
      - platform: template
        id: "this_lock"
        name: "Main Lock"
        optimistic: True
        on_lock:
        - logger.log: "Door Locked!"
        on_unlock:
        - logger.log: "Door Unlocked!"
    homekit:
      lock:
        - id: this_lock
          nfc_id: nfc_spi_module
          on_hk_success:
            - light.toggle: desk_light
          hk_hw_finish: "SILVER"
    ```

## 4. HomeKey

### 4.1 Disclaimer

Like in the case of [HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32), the functionality consists of reverse engineered parts thanks to [@kormax](https://github.com/kormax) and [@kupa22](https://github.com/kupa22) and it is **not** an official implementation by Apple and the functionality or parts of it might or might not break in the future and/or lack official features or internal implementations.

### 4.2 Setup

As it has been mentioned and showcased above, when adding a `lock:` you can also enable homekey functionality by assigning the id for the PN532 component to the `nfc_id` property.

**Important Note:** Modifications were required to the ESPHome PN532 driver components in order to have the implementation functional and only the `pn532_spi` component has been modified at this time. Additionally, it is recommended to keep the `update_interval` very low (< 500ms) for a quicker reaction on detecting and for decreasing the traffic time as sometimes it might fallback to another flow to attest the key which increases the amount of data exchanges. If you experience crashes, try to raise the `update_interval`.

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

## Support

If you found this project helpful, make sure to star the repository ⭐ and if you wish to buy me a coffee, consider sponsoring on [GitHub](https://github.com/sponsors/rednblkx).

## License

This repository consists of multiple licenses since it contains some components([pn532](https://github.com/rednblkx/HAP-ESPHome/tree/main/components/pn532) and [pn532_spi](https://github.com/rednblkx/HAP-ESPHome/tree/main/components/pn532_spi)) originally from the [ESPHome](https://github.com/esphome/esphome) repository, please consult the LICENSE file in each folder in the [components](https://github.com/rednblkx/HAP-ESPHome/tree/main/components) folder.
