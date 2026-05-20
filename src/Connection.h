#pragma once
#include <atomic>
#include <optional>

#include <NimBLEDevice.h>

class NimBLEClient;

namespace BLESomaControl::Internal {

    class Connection {
        bool          active    = false;
        bool          connected = false;
        NimBLEClient* p_client  = nullptr;
        std::mutex    connection_mutex{};

        uint64_t                      address{};
        std::function<void(uint64_t)> cb_free = nullptr;

        template <typename T>
        std::optional<T> readValueT(const NimBLEUUID& service, const NimBLEUUID& characteristic);

    public:
        Connection(uint64_t address, const std::function<void(uint64_t)>& cb_free);
        ~Connection();

        Connection(const Connection&)            = delete;
        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&)      = delete;
        Connection(Connection&& other) noexcept;

        bool connect();

        std::optional<std::string>          readString(const NimBLEUUID& service, const NimBLEUUID& characteristic);
        std::optional<std::vector<uint8_t>> readBytes(const NimBLEUUID& service, const NimBLEUUID& characteristic);
        std::optional<uint16_t>             readUint16(const NimBLEUUID& service, const NimBLEUUID& characteristic);
        bool                                writeValue(const NimBLEUUID& service, const NimBLEUUID& characteristic, const std::vector<uint8_t>& value);
    };
}
