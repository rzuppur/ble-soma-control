/**
 * @file BLESomaControl.h
 * @brief Unofficial library to control SOMA smart home devices over bluetooth.
 * @author Reino Zuppur
 * @copyright Copyright (C) 2026
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <functional>
#include <memory>

#include "Device.h"

namespace BLESomaControl {
    using DeviceCallback = std::function<void(uint64_t)>;

    /**
     * @brief Initializes BLE stack and starts tasks.
     * @param found Called once when a new device is found, use `getDevice(uint64_t)` to access it.
     * @param update Called every time device data has changed, use `getDevice(uint64_t)` to access it.
     * @return success
     */
    bool begin(DeviceCallback found, DeviceCallback update);

    /**
     * @brief Stops scanning and clears found devices. Doesn't delete tasks and ongoing jobs will be finished.
     * @todo Check if NimBLE deinit can be used to actually close BT stack and later init it without issues
     * @return success
     */
    bool pause();

    std::shared_ptr<Device>              getDevice(uint64_t address);
    std::vector<std::shared_ptr<Device>> getDevices();

    namespace Internal {
        class DeviceImpl;
        std::shared_ptr<DeviceImpl>              getDeviceImpl(uint64_t address);
        std::vector<std::shared_ptr<DeviceImpl>> getDeviceImpls();
        std::shared_ptr<DeviceImpl>              createDevice(uint64_t address);
    }
}
