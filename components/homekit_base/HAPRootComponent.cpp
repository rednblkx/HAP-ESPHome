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

        HAPRootComponent::HAPRootComponent(const char* setup_code) : setup_code(setup_code)
        {
            ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
            ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
            ESP_LOGI(TAG, "%s", esp_err_to_name(nvs_flash_init()));
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
            /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
            hap_set_setup_code(setup_code);
            /* Unique four character Setup Id. Default: ES32 */
            hap_set_setup_id("ES32");
        }

        void HAPRootComponent::setup() {
            // hap_http_debug_enable();
            // hap_set_debug_level(HAP_DEBUG_LEVEL_INFO);
            // esp_log_level_set("HAP", ESP_LOG_INFO);
            hap_start();
            ESP_LOGI(TAG, "HAP Bridge started!");
        }

        void HAPRootComponent::loop() {
        }

        void HAPRootComponent::dump_config() {
        }

    }  // namespace homekit
}  // namespace esphome