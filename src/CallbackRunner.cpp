#include "CallbackRunner.h"

#include <atomic>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static auto TAG = "BSC";

namespace {
    constexpr auto CALLBACK_QUEUE_SIZE  = 10;
    constexpr auto CALLBACK_TASKS_COUNT = 2;

    std::mutex state_mutex{};

    BLESomaControl::DeviceCallback cb_found  = nullptr;
    BLESomaControl::DeviceCallback cb_update = nullptr;

    QueueHandle_t callback_queue;
    TaskHandle_t  task_handles[CALLBACK_TASKS_COUNT] = {nullptr};
    std::atomic   running                            = false;

    [[noreturn]] void task(void*)
    {
        using namespace BLESomaControl::Internal::CallbackRunner;
        while (true) {
            Item item{};
            if (xQueueReceive(callback_queue, &item, portMAX_DELAY) == pdTRUE) {
                if (running) {
                    if (item.type == Type::FOUND && cb_found) cb_found(item.address);
                    if (item.type == Type::UPDATE && cb_update) cb_update(item.address);
                }
            }
        }
    }
}

namespace BLESomaControl::Internal::CallbackRunner {
    bool begin(DeviceCallback found, DeviceCallback update)
    {
        std::lock_guard state_lock(state_mutex);
        if (running) return false;

        cb_found  = std::move(found);
        cb_update = std::move(update);

        if (!callback_queue) {
            callback_queue = xQueueCreate(CALLBACK_QUEUE_SIZE, sizeof(Item));
            if (!callback_queue) return false;
        }

        for (int i = 0; i < CALLBACK_TASKS_COUNT; i++) {
            if (task_handles[i]) continue;
            const auto success = xTaskCreatePinnedToCore(task, "cbRunner", 4096, nullptr, 0, &task_handles[i], xPortGetCoreID()) == pdPASS;
            if (!success) {
                for (int j = 0; j < i; j++) {
                    vTaskDelete(task_handles[j]);
                    task_handles[j] = nullptr;
                }
                return false;
            }
        }

        running = true;
        return true;
    }

    bool pause()
    {
        std::lock_guard state_lock(state_mutex);
        if (!running) return false;
        running = false;

        return true;
    }

    bool queueCallback(const Item& item)
    {
        if (!callback_queue) {
            ESP_LOGE(TAG, "CallbackRunner callback queued before begin()");
            return false;
        }

        const auto success = xQueueSendToBack(callback_queue, &item, 0) == pdPASS;
        if (!success) {
            ESP_LOGD(TAG, "CallbackRunner queue full");
        }
        return success;
    }
}
