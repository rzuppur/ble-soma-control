#pragma once
#include <optional>

namespace BLESomaControl {
    enum class DeviceState { CLOSED, CLOSING, OPEN, OPENING, UNKNOWN, UNAVAILABLE }; // matches https://www.home-assistant.io/integrations/cover/

    class Device {
    protected:
        explicit Device(const uint64_t address)
            : address(address)
        {
        }

    public:
        const uint64_t address;

        Device(const Device&)            = delete;
        Device& operator=(const Device&) = delete;

        virtual ~Device() = default;

        virtual std::string             name()                 = 0;
        virtual std::string             firmware_version()     = 0;
        virtual std::optional<uint8_t>  position()             = 0; /* 0=open  100=closed */
        virtual DeviceState             state()                = 0;
        virtual std::optional<uint8_t>  battery()              = 0; /* 0=empty 100=full */
        virtual std::optional<uint16_t> solar_panel_voltage()  = 0; /* mV on solar panel */
        virtual std::optional<uint8_t>  light_level()          = 0; /* not tested, don't have access to v3 device with solar */
        virtual std::optional<float>    internal_temperature() = 0; /* board temperature in Celsius */
        virtual std::optional<int8_t>   rssi()                 = 0; /* averaged signal strength dBm */

        virtual bool commandIdentify() const         = 0; /* ask the device to beep */
        virtual bool commandPosition(uint8_t target) = 0; /* set shades position 0=open 100=closed */
        virtual bool commandStop() const             = 0; /* stop shades */

        static std::string state_text(const DeviceState state)
        {
            switch (state) {
            case DeviceState::CLOSED:
                return "closed";
            case DeviceState::CLOSING:
                return "closing";
            case DeviceState::OPEN:
                return "open";
            case DeviceState::OPENING:
                return "opening";
            case DeviceState::UNKNOWN:
                return "unknown";
            case DeviceState::UNAVAILABLE:
                return "unavailable";
            default:
                return "error";
            }
        }
    };
}
