#include "HAPRootComponent.h"

namespace esphome
{
    namespace homekit
    {
        /* Mandatory identify routine for the accessory.
        * In a real accessory, something like LED blink should be implemented
        * got visual identification
        */
        static int acc_identify(hap_acc_t *ha)
        {
            ESP_LOGI("HAP", "Accessory identified");
            return HAP_SUCCESS;
        }

        void HAPRootComponent::factory_reset() {
            hap_reset_pairings();
        }
        
        void HAPRootComponent::hap_thread(void* arg)
        {
            HAPRootComponent* root = (HAPRootComponent*)arg;
            hap_acc_t* accessory;
            /* Initialize the HAP core */
            hap_init(HAP_TRANSPORT_WIFI);

            /* Initialise the mandatory parameters for Accessory which will be added as
            * the mandatory services internally
            */
            hap_cfg_t hap_cfg;
            hap_get_config(&hap_cfg);
            hap_cfg.task_stack_size = 8192;
            hap_cfg.task_priority = 2;
            hap_set_config(&hap_cfg);
            hap_acc_cfg_t cfg = {
                .name = "ESPH Bridge",
                .model = "HAP-ESPHome",
                .manufacturer = "rednblkx",
                .serial_num = "abcdefg",
                .fw_rev = "0.1.0",
                .hw_rev = "1.0",
                .pv = "1.1.0",
                .cid = HAP_CID_BRIDGE,
                .identify_routine = acc_identify,
            };

            /* Create accessory object */
            accessory = hap_acc_create(&cfg);
            if (!accessory) {
                ESP_LOGE(TAG, "Failed to create accessory");
                hap_acc_delete(accessory);
                vTaskDelete(NULL);
            }

            /* Add a dummy Product Data */
            uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
            hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

            /* Add Wi-Fi Transport service required for HAP Spec R16 */
            hap_acc_add_wifi_transport_service(accessory, 0);

            /* Add the Accessory to the HomeKit Database */
            hap_add_accessory(accessory);
            #ifdef USE_LIGHT
            root->lightEntity->setup();
            #endif
            #ifdef USE_LOCK
            root->lockEntity->setup();
            #endif
            #ifdef USE_SENSOR
            root->sensorEntity->setup();
            #endif
            #ifdef USE_SWITCH
            root->switchEntity->setup();
            #endif
            /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
            hap_set_setup_code("111-22-333");
            /* Unique four character Setup Id. Default: ES32 */
            hap_set_setup_id("ES32");

            // hap_http_debug_enable();
            hap_set_debug_level(HAP_DEBUG_LEVEL_ASSERT);
            hap_start();

            /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
            vTaskDelete(NULL);
        }

        HAPRootComponent::HAPRootComponent(bool exposeAll, TemperatureUnits tempUnits) : exposeAll(exposeAll), tempUnits(tempUnits)
        {
            ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
            ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
            ESP_LOGI(TAG, "%s", esp_err_to_name(nvs_flash_init()));
            include_lights = std::vector<light::LightState*>();
            exclude_lights = std::vector<light::LightState*>();
            include_sensors = std::vector<sensor::Sensor*>();
            exclude_sensors = std::vector<sensor::Sensor*>();
            include_switches = std::vector<switch_::Switch*>();
            exclude_switches = std::vector<switch_::Switch*>();
            include_locks = std::vector<lock::Lock*>();
            exclude_locks = std::vector<lock::Lock*>();
        }

        void HAPRootComponent::setup() {
            #ifdef USE_LIGHT
            lightEntity = new LightEntity(exposeAll, include_lights, exclude_lights);
            #endif
            #ifdef USE_SWITCH
            switchEntity = new SwitchEntity(exposeAll, include_switches, exclude_switches);
            #endif
            #ifdef USE_SENSOR
            sensorEntity = new SensorEntity(exposeAll, include_sensors, exclude_sensors);
            #endif
            #ifdef USE_SENSOR
            lockEntity = new LockEntity(exposeAll, include_locks, exclude_locks);
            #endif
            #ifdef USE_CLIMATE
            climateEntity = new ClimateEntity(exposeAll, include_climates, exclude_climates);
            climateEntity->setUnits(tempUnits);
            #endif
            xTaskCreate(hap_thread, "hap_task", 4 * 1024, this, 2, NULL);
        }

        void HAPRootComponent::loop() {
        }

        void HAPRootComponent::dump_config() {
        }

    }  // namespace homekit
}  // namespace esphome