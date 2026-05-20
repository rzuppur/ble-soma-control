#include "DeviceScanner.h"

#include <atomic>

#include "BLESomaControl.h"
#include "CallbackRunner.h"
#include "ConnectionPool.h"
#include "DeviceImpl.h"
#include "NimBLEDevice.h"

static auto TAG = "BSC";

namespace {
    std::mutex state_mutex{};

    TaskHandle_t task_handle = nullptr;
    std::atomic  running     = false;

    class ScanCallbacks final : public NimBLEScanCallbacks {
        void onResult(const NimBLEAdvertisedDevice* ble_device) override
        {
            if (!ble_device->haveManufacturerData()) return;
            const auto manufacturer_data = ble_device->getManufacturerData();
            if (manufacturer_data.size() >= 5 && manufacturer_data[0] == 0x70 && manufacturer_data[1] == 0x03) {
                const auto address = ble_device->getAddress();
                auto       device  = BLESomaControl::Internal::getDeviceImpl(address);
                if (!device) {
                    device = BLESomaControl::Internal::createDevice(address);
                    ESP_LOGV(TAG, "Found        %llu", static_cast<uint64_t>(address));
                    BLESomaControl::Internal::CallbackRunner::queueCallback({address, BLESomaControl::Internal::CallbackRunner::Type::FOUND});
                }
                device->updateFromAdvertisement(manufacturer_data, ble_device->getRSSI());
            }
        }
    };

    ScanCallbacks scan_callbacks;

    void startScan()
    {
        auto* p_scan = NimBLEDevice::getScan();
        if (!p_scan->isScanning()) {
            p_scan->setInterval(97);
            p_scan->setWindow(87);
            p_scan->setMaxResults(0);
            p_scan->setScanCallbacks(&scan_callbacks, true);
            p_scan->setDuplicateFilter(false);
            if (p_scan->start(0, false)) {
                ESP_LOGV(TAG, "Started scanning");
            } else {
                ESP_LOGV(TAG, "Failed to start scanning");
            }
        }
    }

    void stopScan()
    {
        auto* p_scan = NimBLEDevice::getScan();
        if (p_scan->isScanning()) p_scan->stop();
        p_scan->setScanCallbacks(nullptr);
        p_scan->clearResults();
        ESP_LOGV(TAG, "Stopped scanning");
    }

    [[noreturn]] void task(void*)
    {
        while (true) {
            if (running && !BLESomaControl::Internal::connectionActive()) startScan();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

namespace BLESomaControl::Internal::DeviceScanner {
    bool begin()
    {
        std::lock_guard state_lock(state_mutex);
        if (running) return false;

        if (!task_handle) {
            const auto success = xTaskCreatePinnedToCore(task, "dvScanner", 4096, nullptr, 0, &task_handle, xPortGetCoreID()) == pdPASS;
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

        stopScan();

        return true;
    }
}
