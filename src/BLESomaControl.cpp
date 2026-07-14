#include "BLESomaControl.h"

#include <mutex>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "CallbackRunner.h"
#include "DeviceController.h"
#include "DeviceImpl.h"
#include "DeviceScanner.h"
#include "NimBLEDevice.h"

static auto TAG = "BSC";

namespace {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    bool       running = false;
    std::mutex state_mutex{};
    std::mutex devices_mutex{};

    std::unordered_map<uint64_t, std::shared_ptr<BLESomaControl::Internal::DeviceImpl>> devices{};

    void bleInit()
    {
        NimBLEDevice::init("");
        NimBLEDevice::setPower(9, NimBLETxPowerType::All);
        NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);
    }
}

namespace BLESomaControl {
    bool begin(DeviceCallback found, DeviceCallback update)
    {
        std::lock_guard state_lock(state_mutex);
        if (running) return false;
        bleInit();

        if (!Internal::DeviceController::begin()) return false;
        if (!Internal::CallbackRunner::begin(std::move(found), std::move(update))) {
            Internal::DeviceController::pause();
            return false;
        }
        if (!Internal::DeviceScanner::begin()) {
            Internal::DeviceController::pause();
            Internal::CallbackRunner::pause();
            return false;
        }

        ESP_LOGD(TAG, "BLESomaControl started");
        running = true;
        return true;
    }

    bool pause()
    {
        std::lock_guard state_lock(state_mutex);
        if (!running) return false;

        Internal::DeviceScanner::pause();
        Internal::DeviceController::pause();
        Internal::CallbackRunner::pause();

        {
            std::lock_guard devices_lock(devices_mutex);
            devices.clear();
        }

        ESP_LOGD(TAG, "BLESomaControl paused");
        running = false;
        return true;
    }

    std::shared_ptr<Internal::DeviceImpl> Internal::getDeviceImpl(const uint64_t address)
    {
        std::lock_guard devices_lock(devices_mutex);
        if (const auto it = devices.find(address); it != devices.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Internal::DeviceImpl>> Internal::getDeviceImpls()
    {
        std::lock_guard                          devices_lock(devices_mutex);
        std::vector<std::shared_ptr<DeviceImpl>> result;
        for (auto& device : devices | std::views::values) {
            result.push_back(device);
        }
        return result;
    }

    std::shared_ptr<Internal::DeviceImpl> Internal::createDevice(uint64_t address)
    {
        return devices.emplace(address, std::make_shared<DeviceImpl>(address)).first->second;
    }

    std::shared_ptr<Device> getDevice(const uint64_t address) { return Internal::getDeviceImpl(address); }

    std::vector<std::shared_ptr<Device>> getDevices()
    {
        std::lock_guard                      devices_lock(devices_mutex);
        std::vector<std::shared_ptr<Device>> result;
        for (auto& device : devices | std::views::values) {
            result.push_back(device);
        }
        return result;
    }
}
