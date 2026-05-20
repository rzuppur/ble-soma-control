#pragma once
#include <atomic>
#include <mutex>
#include <optional>
#include <string>

#include "Device.h"

namespace BLESomaControl::Internal {
    class Connection;

    class DeviceImpl final : public Device {
        std::atomic<uint64_t> last_ping{};
        std::atomic<uint64_t> last_read{};
        std::atomic<uint64_t> last_position_command{};

        std::mutex data_mutex{};
        bool       data_changed = true;

        bool                    is_v3 = false;
        std::string             data_name{};
        std::string             data_firmware_version{};
        std::optional<uint8_t>  data_position{};
        std::optional<uint8_t>  data_position_target{};
        DeviceState             data_state = DeviceState::UNKNOWN;
        std::optional<uint8_t>  data_battery{};
        std::optional<uint16_t> data_solar_panel_voltage{};
        std::optional<uint8_t>  data_light_level{};
        std::optional<float>    data_internal_temperature{};
        std::optional<int8_t>   data_rssi{};
        std::optional<float>    data_rssi_avg{};

        void setName(const std::string& name);
        void setFirmwareVersion(const std::string& firmware_version);
        void setPosition(uint8_t position);
        void setState(DeviceState state);
        void setBattery(uint8_t battery);
        void setSolarPanelVoltage(uint16_t solar_panel_voltage);
        void setLightLevel(uint8_t light_level);
        void setInternalTemperature(float internal_temperature);
        void setRSSI(int8_t rssi);

        static std::optional<std::string> readV3String(Connection& connection, uint8_t command);
        static std::optional<float>       readV3Float(Connection& connection, uint8_t command);
        static std::optional<uint8_t>     readV3Uint8(Connection& connection, uint8_t command);

    public:
        std::atomic<bool> update_queued = false;

        explicit DeviceImpl(uint64_t address);
        ~DeviceImpl() override = default;

        std::string             name() override;
        std::string             firmware_version() override;
        std::optional<uint8_t>  position() override;
        DeviceState             state() override;
        std::optional<uint8_t>  battery() override;
        std::optional<uint16_t> solar_panel_voltage() override;
        std::optional<uint8_t>  light_level() override;
        std::optional<float>    internal_temperature() override;
        std::optional<int8_t>   rssi() override;

        bool commandIdentify() const override;
        bool commandPosition(uint8_t target) override;
        bool commandStop() const override;

        bool consumeChanged();
        void updateFromAdvertisement(const std::string& manufacturer_data, int8_t rssi);
        void update();
        void tick();
    };
}
