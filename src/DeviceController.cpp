#include "DeviceController.h"

#include <atomic>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "BLESomaControl.h"
#include "CallbackRunner.h"
#include "DeviceImpl.h"

namespace {
    std::mutex   state_mutex{};
    TaskHandle_t task_handle = nullptr;
    std::atomic  running     = false;

    [[noreturn]] void task(void*)
    {
        while (true) {
            for (const auto& p_device : BLESomaControl::Internal::getDeviceImpls()) {
                p_device->tick();
                if (p_device->update_queued) {
                    p_device->update();
                    p_device->update_queued = false;
                }
                if (p_device->consumeChanged()) {
                    BLESomaControl::Internal::CallbackRunner::queueCallback({p_device->address, BLESomaControl::Internal::CallbackRunner::Type::UPDATE});
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

namespace BLESomaControl::Internal::DeviceController {
    bool begin()
    {
        std::lock_guard state_lock(state_mutex);
        if (running) return false;

        if (!task_handle) {
            const auto success = xTaskCreatePinnedToCore(task, "dvController", 4096, nullptr, 0, &task_handle, xPortGetCoreID()) == pdPASS;
            if (!success) return false;
        }

        running = true;
        return true;
    }

    bool end()
    {
        std::lock_guard state_lock(state_mutex);
        if (!running) return false;
        running = false;

        return true;
    }
}
