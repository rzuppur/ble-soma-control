#include <BLESomaControl.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

static auto TAG = "BSC_EXAMPLE";

void logHeap() { ESP_LOGV(TAG, "Free heap %ikb", esp_get_free_heap_size() / 1000); }

extern "C" [[noreturn]] void app_main()
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("BSC", ESP_LOG_VERBOSE);
    esp_log_level_set("BSC_EXAMPLE", ESP_LOG_VERBOSE);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    vTaskDelay(pdMS_TO_TICKS(3000));

    logHeap();
    BLESomaControl::begin(nullptr, nullptr);
    logHeap();

    while (true) {
        const auto devices = BLESomaControl::getDevices();
        ESP_LOGI(TAG, "%d devices", devices.size());
        for (auto& p_device : devices) {
            ESP_LOGD(
                TAG,
                "%s @%d %s (RSSI %i dBm)",
                p_device->name().c_str(),
                p_device->position().value_or(-1),
                BLESomaControl::Device::state_text(p_device->state()).c_str(),
                p_device->rssi().value_or(-1));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
