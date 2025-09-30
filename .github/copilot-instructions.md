# HAP-ESPHome

HAP-ESPHome provides HomeKit Accessory Protocol (HAP) support for ESP32 devices via ESPHome external components. This enables direct control of ESP32 devices from the Apple Home app without intermediary services.

**Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.**

## Working Effectively

### Prerequisites and Installation
- Install ESPHome and PlatformIO Core:
  ```bash
  pip install --upgrade platformio esphome
  ```
- Verify installation:
  ```bash
  esphome version
  ```

### Quick Development Setup
- Create secrets file for testing (always required):
  ```bash
  echo 'wifi_ssid: "ssid"' > secrets.yaml
  echo 'wifi_password: "password"' >> secrets.yaml
  ```

### Build and Validation Commands
- **Validate configuration WITHOUT building** (fastest validation, ~0.4 seconds):
  ```bash
  esphome config test.yaml
  ```
- **Full compilation** (takes 15-45 minutes on first build, NEVER CANCEL, set timeout to 60+ minutes):
  ```bash
  esphome compile test.yaml
  ```
- **Clean build artifacts**:
  ```bash
  esphome clean test.yaml
  ```

### CRITICAL Build Requirements
- **NEVER CANCEL builds or long-running commands** - Initial builds take 15-45 minutes due to ESP-IDF component downloads
- **Always set timeouts of 60+ minutes** for any compile commands
- **Build failures may occur due to network restrictions** - PlatformIO registry access required for full compilation
- **Configuration validation always works** and should be used for quick testing

## Validation
- **Always run configuration validation first** before attempting compilation:
  ```bash
  esphome config <config-file>.yaml
  ```
- **Validate all example configurations** when making component changes:
  ```bash
  esphome config test.yaml
  esphome config lights-c3.yaml  
  esphome config sensor-c3.yaml
  esphome config switch-c3.yaml
  esphome config fan-c3.yaml
  ```
- **Test both simple and complex configurations** - test.yaml includes all component types, other configs test specific functionality
- **Always create a secrets.yaml file** before running any esphome commands - this is required for all configurations

## Component Development
### Component Structure
- **homekit_base**: Core HomeKit bridge functionality (C++/Python)
- **homekit**: Accessory logic for lights, locks, sensors, switches, fans (C++/Python)
- **pn532**: NFC component for HomeKey functionality (C++/Python)  
- **pn532_spi**: SPI interface for PN532 (C++/Python)

### Configuration Requirements
**All configurations must include:**
```yaml
esp32:
  board: <board-type>
  framework:
    type: esp-idf
    version: 5.3.1
    platform_version: 6.8.1
    sdkconfig_options:
      CONFIG_COMPILER_OPTIMIZATION_SIZE: y
      CONFIG_LWIP_MAX_SOCKETS: "16"
      CONFIG_MBEDTLS_HKDF_C: y

external_components:
  source: github://rednblkx/HAP-ESPHome@main
  refresh: 0s
```

### Supported Entity Types
- **Light**: On/Off, Brightness, RGB, Color Temperature
- **Lock**: Lock/Unlock, HomeKey support (requires pn532_spi)
- **Switch**: On/Off
- **Sensor**: Temperature, Humidity, Illuminance, Air Quality, CO2, CO, PM10, PM2.5
- **Fan**: On/Off
- **Cover**: Garage Door Opener

## Common Tasks

### Repo Structure
```
HAP-ESPHome/
├── .github/workflows/main.yml    # CI pipeline
├── components/                   # ESPHome external components
│   ├── homekit/                 # Main HomeKit accessory logic
│   ├── homekit_base/            # Core HomeKit bridge
│   ├── pn532/                   # NFC component
│   └── pn532_spi/               # SPI NFC interface
├── README.md                    # Main documentation
├── test.yaml                    # Comprehensive test configuration
├── lights-c3.yaml              # Light control example
├── sensor-c3.yaml              # Sensor monitoring example
├── switch-c3.yaml              # Switch control example
├── fan-c3.yaml                 # Fan control example
└── homekey-test-*.yaml          # HomeKey functionality examples
```

### Key Configuration Files

#### test.yaml (Comprehensive Test)
Complete configuration testing all component types including:
- Lock with HomeKey (pn532_spi integration)
- Multiple lights (RGB LED strip, binary)
- Multiple sensors (temperature with metadata)
- Switch with HomeKit integration
- Factory reset button

#### lights-c3.yaml (Simple Light Control)
Basic RGB and binary light control without HomeKey functionality.

#### sensor-c3.yaml (Sensor Monitoring)
Temperature and humidity sensor integration with HomeKit.

### Working with HomeKey
**HomeKey requires PN532 over SPI only** (other protocols not supported):
```yaml
spi:
  clk_pin: 4
  miso_pin: 5
  mosi_pin: 6

pn532_spi:
  id: nfc_spi_module
  cs_pin: 7
  update_interval: 100ms  # Keep <= 500ms for quick reactions

homekit:
  lock:
    - id: my_lock
      nfc_id: nfc_spi_module
      hk_hw_finish: "SILVER"  # BLACK, SILVER, GOLD, TAN
```

### ESP-IDF Framework Notes
- **Framework version 5+ required** (5.3.1 tested and working)
- **Platform version 6.8.1 recommended**
- **CONFIG_COMPILER_OPTIMIZATION_SIZE helps with memory constraints**
- **CONFIG_LWIP_MAX_SOCKETS: "16" required for HomeKit networking**
- **CONFIG_MBEDTLS_HKDF_C required for HomeKit cryptography**

### Build Artifacts Location
- Build files: `.esphome/build/<device-name>/`
- Generated C++ source: `.esphome/build/<device-name>/src/main.cpp`
- PlatformIO config: `.esphome/build/<device-name>/platformio.ini`
- ESP-IDF config: `.esphome/build/<device-name>/sdkconfig.*`

### External Dependencies
Components automatically download from:
- **esp-homekit-sdk**: https://github.com/rednblkx/esp-homekit-sdk (multiple IDF components)
- **HK-HomeKit-Lib**: https://github.com/rednblkx/HK-HomeKit-Lib.git (HomeKey specific)

### Common Warnings (Expected)
- **"Using the '_' (underscore) character in the hostname is discouraged"** - Can be ignored for test configurations
- **"The selected ESP-IDF framework version is not the recommended one"** - Expected, version 5+ is intentional
- **"GPIO8 is a strapping PIN"** - Expected for ESP32-C3 examples
- **"Can't decode message length"** during HomeKey operation - Normal PN532 operation

### Error Resolution
- **Build fails with network errors**: Compilation requires internet access for ESP-IDF components
- **"section '.iram0.text' will not fit"**: Disable large components like Bluetooth to free RAM
- **Missing secrets.yaml**: Always create this file before running esphome commands
- **Component validation errors**: Check that external_components source is correct

### CI/CD Pipeline
The `.github/workflows/main.yml` runs:
1. Sets up Python 3.11
2. Installs PlatformIO Core and ESPHome
3. Creates dummy secrets.yaml
4. Compiles test.yaml configuration

**Build timing**: CI builds typically complete in 5-15 minutes due to caching.