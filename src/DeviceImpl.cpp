#include "DeviceImpl.h"

#include <chrono>
#include <cmath>
#include <utility>

#include "ConnectionPool.h"
#include "DeviceController.h"
#include "Text.h"

namespace {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    constexpr auto DEVICE_LOST_TIMEOUT      = 20s;
    constexpr auto POSITION_COMMAND_TIMEOUT = 10s;
    constexpr auto PERIODIC_READ_INTERVAL   = 5min + 4s;

    const NimBLEUUID SVC_V2_DEVICE_INFO("180A");
    const NimBLEUUID CHR_V2_FIRMWARE_VERSION("2A28");

    const NimBLEUUID SVC_V2_MOTOR("00001861-B87F-490C-92CB-11BA5EA5167C");
    const NimBLEUUID CHR_V2_BATTERY("0000BA71-B87F-490C-92CB-11BA5EA5167C");
    const NimBLEUUID CHR_V2_SHADES_TARGET("00001526-B87F-490C-92CB-11BA5EA5167C");
    const NimBLEUUID CHR_V2_MOTOR_CONTROL("00001530-B87F-490C-92CB-11BA5EA5167C");
    const NimBLEUUID CHR_V2_IDENTIFY("00001531-B87F-490C-92CB-11BA5EA5167C");

    const NimBLEUUID SVC_V2_DEVICE("00001890-B87F-490C-92CB-11BA5EA5167C");
    const NimBLEUUID CHR_V2_LIGHT_LEVEL("00001894-B87F-490C-92CB-11BA5EA5167C");

    const NimBLEUUID SVC_V3_COMMAND("8998A466-A65F-4D49-90B2-927F41C55190");
    const NimBLEUUID CHR_V3_WRITE("8998A466-A65F-4D49-90B2-927F41C55191");
    const NimBLEUUID CHR_V3_READ("8998A466-A65F-4D49-90B2-927F41C55192");

    constexpr uint8_t CMD_STOP                 = 0x03;
    constexpr uint8_t CMD_SET_POSITION         = 0x08;
    constexpr uint8_t CMD_GET_NAME             = 0x14;
    constexpr uint8_t CMD_GET_TEMPERATURE      = 0x16;
    constexpr uint8_t CMD_IDENTIFY             = 0x17;
    constexpr uint8_t CMD_GET_FIRMWARE_VERSION = 0x27;
    constexpr uint8_t CMD_GET_LIGHT_LEVEL      = 0x3E;
}

namespace BLESomaControl::Internal {
    void DeviceImpl::setName(const std::string& name)
    {
        std::lock_guard data_lock(data_mutex);
        if (name != data_name) {
            data_name    = name;
            data_changed = true;
        }
    }

    void DeviceImpl::setFirmwareVersion(const std::string& firmware_version)
    {
        std::lock_guard data_lock(data_mutex);
        if (firmware_version != data_firmware_version) {
            data_firmware_version = firmware_version;
            data_changed          = true;
        }
    }

    void DeviceImpl::setPosition(const uint8_t position)
    {
        std::lock_guard data_lock(data_mutex);
        if (!data_position || position != *data_position) {
            data_position = position;
            data_changed  = true;
        }
    }

    void DeviceImpl::setState(const DeviceState state)
    {
        std::lock_guard data_lock(data_mutex);
        if (state != data_state) {
            data_state   = state;
            data_changed = true;
        }
    }

    void DeviceImpl::setBattery(const uint8_t battery)
    {
        std::lock_guard data_lock(data_mutex);
        if (!data_battery || battery != *data_battery) {
            data_battery = battery;
            data_changed = true;
        }
    }

    void DeviceImpl::setSolarPanelVoltage(uint16_t solar_panel_voltage)
    {
        std::lock_guard data_lock(data_mutex);
        if (!data_solar_panel_voltage || solar_panel_voltage != *data_solar_panel_voltage) {
            data_solar_panel_voltage = solar_panel_voltage;
            data_changed             = true;
        }
    }

    void DeviceImpl::setLightLevel(uint8_t light_level)
    {
        std::lock_guard data_lock(data_mutex);
        if (!data_light_level || light_level != *data_light_level) {
            data_light_level = light_level;
            data_changed     = true;
        }
    }

    void DeviceImpl::setInternalTemperature(float internal_temperature)
    {
        std::lock_guard data_lock(data_mutex);
        if (!data_internal_temperature || internal_temperature != *data_internal_temperature) {
            data_internal_temperature = internal_temperature;
            data_changed              = true;
        }
    }

    void DeviceImpl::setRSSI(int8_t rssi)
    {
        std::lock_guard data_lock(data_mutex);
        data_rssi_avg = data_rssi_avg.value_or(rssi);
        data_rssi_avg = *data_rssi_avg + (rssi - *data_rssi_avg) * 0.1f;
        rssi          = std::round(*data_rssi_avg);
        if (!data_rssi || std::abs(rssi - *data_rssi) > 3) {
            data_rssi    = rssi;
            data_changed = true;
        }
    }

    std::optional<std::string> DeviceImpl::readV3String(Connection& connection, uint8_t command)
    {
        if (connection.writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {command, 0x00})) {
            auto response = connection.readString(SVC_V3_COMMAND, CHR_V3_READ);
            if (response && !response->empty() && response->at(0) == command) {
                response->erase(0, 2);
                return *response;
            }
        }
        return std::nullopt;
    }

    std::optional<float> DeviceImpl::readV3Float(Connection& connection, uint8_t command)
    {
        if (connection.writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {command, 0x00})) {
            const auto response = connection.readBytes(SVC_V3_COMMAND, CHR_V3_READ);
            if (response && response->size() >= 6 && response->at(0) == command) {
                float result;
                std::memcpy(&result, response->data() + 2, sizeof(float));
                return result;
            }
        }
        return std::nullopt;
    }

    std::optional<uint8_t> DeviceImpl::readV3Uint8(Connection& connection, uint8_t command)
    {
        if (connection.writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {command, 0x00})) {
            const auto response = connection.readBytes(SVC_V3_COMMAND, CHR_V3_READ);
            if (response && response->size() >= 3 && response->at(0) == command) {
                uint8_t result;
                std::memcpy(&result, response->data() + 2, sizeof(uint8_t));
                return result;
            }
        }
        return std::nullopt;
    }

    DeviceImpl::DeviceImpl(const uint64_t address)
        : Device(address)
    {
        last_ping = esp_timer_get_time();
    }

    bool DeviceImpl::consumeChanged()
    {
        std::lock_guard data_lock(data_mutex);
        return std::exchange(data_changed, false);
    }

    std::string DeviceImpl::name()
    {
        std::lock_guard data_lock(data_mutex);
        return data_name;
    }

    std::string DeviceImpl::firmware_version()
    {
        std::lock_guard data_lock(data_mutex);
        return data_firmware_version;
    }

    std::optional<uint8_t> DeviceImpl::position()
    {
        std::lock_guard data_lock(data_mutex);
        return data_position;
    }

    DeviceState DeviceImpl::state()
    {
        std::lock_guard data_lock(data_mutex);
        return data_state;
    }

    std::optional<uint8_t> DeviceImpl::battery()
    {
        std::lock_guard data_lock(data_mutex);
        return data_battery;
    }

    std::optional<uint16_t> DeviceImpl::solar_panel_voltage()
    {
        std::lock_guard data_lock(data_mutex);
        return data_solar_panel_voltage;
    }

    std::optional<uint8_t> DeviceImpl::light_level()
    {
        std::lock_guard data_lock(data_mutex);
        return data_light_level;
    }

    std::optional<float> DeviceImpl::internal_temperature()
    {
        std::lock_guard data_lock(data_mutex);
        return data_internal_temperature;
    }

    std::optional<int8_t> DeviceImpl::rssi()
    {
        std::lock_guard data_lock(data_mutex);
        return data_rssi;
    }

    void DeviceImpl::tick()
    {
        constexpr auto lost_timeout_us             = duration_cast<microseconds>(DEVICE_LOST_TIMEOUT).count();
        constexpr auto position_command_timeout_us = duration_cast<microseconds>(POSITION_COMMAND_TIMEOUT).count();
        const uint64_t last_contact                = std::max(last_ping, last_read);
        const auto     now                         = esp_timer_get_time();

        if (!data_position) {
            // Hasn't been read yet
            setState(DeviceState::UNKNOWN);
        } else if (now - last_contact >= lost_timeout_us) {
            // Has been read, device lost timeout
            setState(DeviceState::UNAVAILABLE);
        } else if (!data_position_target || *data_position_target == *data_position || now - last_position_command >= position_command_timeout_us) {
            // No active move in progress or position command has timed out
            data_position_target = std::nullopt;
            setState(*data_position < 100 ? DeviceState::OPEN : DeviceState::CLOSED);
        } else {
            // Move in progress
            setState(*data_position_target > *data_position ? DeviceState::CLOSING : DeviceState::OPENING);
        }

        // Periodically connect to the device and read data that isn't available from advertisements
        if (update_queued) return;
        if (!last_read || esp_timer_get_time() - last_read >= duration_cast<microseconds>(PERIODIC_READ_INTERVAL).count()) {
            update_queued = true;
        }
    }

    void DeviceImpl::updateFromAdvertisement(const std::string& manufacturer_data, const int8_t rssi)
    {
        last_ping = esp_timer_get_time();
        setRSSI(rssi);
        if (manufacturer_data[2] >= 16) {
            is_v3 = true;
            setPosition(static_cast<int8_t>(manufacturer_data[3]));
            setBattery(static_cast<int8_t>(manufacturer_data[4]));
        } else {
            is_v3 = false;
            setPosition(static_cast<int8_t>(manufacturer_data[4]));
            std::string nameValue = manufacturer_data.substr(6);
            nameValue             = trimString(nameValue);
            nameValue.pop_back();
            setName(nameValue);
        }
    }

    void DeviceImpl::update()
    {
        auto connection = getConnection(address);
        if (connection) {
            if (is_v3) {
                // Name
                const auto name = readV3String(*connection, CMD_GET_NAME);
                if (name) setName(*name);

                // Firmware version
                const auto version = readV3String(*connection, CMD_GET_FIRMWARE_VERSION);
                if (version) setFirmwareVersion(*version);

                // Internal temperature
                const auto temperature = readV3Float(*connection, CMD_GET_TEMPERATURE);
                if (temperature) setInternalTemperature(*temperature);

                // Light level
                const auto light = readV3Uint8(*connection, CMD_GET_LIGHT_LEVEL);
                if (light) setLightLevel(*light);

            } else {
                // Battery level
                const auto voltage = connection->readUint16(SVC_V2_MOTOR, CHR_V2_BATTERY);
                if (voltage) {
                    if (*voltage < 360)
                        setBattery(0);
                    else if (*voltage > 410)
                        setBattery(100);
                    else
                        setBattery((*voltage - 360) * 10 / 5);
                }

                // Firmware version
                const auto version = connection->readString(SVC_V2_DEVICE_INFO, CHR_V2_FIRMWARE_VERSION);
                if (version) setFirmwareVersion(*version);

                // Solar panel voltage
                const auto solar = connection->readUint16(SVC_V2_DEVICE, CHR_V2_LIGHT_LEVEL);
                if (solar) setSolarPanelVoltage(*solar);
            }

            last_read = esp_timer_get_time();
        }
    }

    bool DeviceImpl::commandIdentify() const
    {
        auto connection = getConnection(address);
        if (connection) {
            if (is_v3) return connection->writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {CMD_IDENTIFY, 0x00});
            return connection->writeValue(SVC_V2_MOTOR, CHR_V2_IDENTIFY, {0x00});
        }
        return false;
    }

    bool DeviceImpl::commandPosition(uint8_t target)
    {
        auto connection = getConnection(address);
        if (connection) {
            if (is_v3) return connection->writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {CMD_SET_POSITION, 0x01, target});
            if (connection->writeValue(SVC_V2_MOTOR, CHR_V2_SHADES_TARGET, {target})) {
                data_position_target  = target;
                last_position_command = esp_timer_get_time();
                return true;
            }
        }
        return false;
    }

    bool DeviceImpl::commandStop() const
    {
        auto connection = getConnection(address);
        if (connection) {
            if (is_v3) return connection->writeValue(SVC_V3_COMMAND, CHR_V3_WRITE, {CMD_STOP, 0x00});
            return connection->writeValue(SVC_V2_MOTOR, CHR_V2_MOTOR_CONTROL, {0x00});
        }
        return false;
    }
}
