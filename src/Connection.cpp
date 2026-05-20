#include "Connection.h"

#include <NimBLEDevice.h>

static auto TAG = "BSC";

namespace {
    constexpr auto MAX_CONNECTION_RETRIES = 3;
}

namespace BLESomaControl::Internal {
    Connection::Connection(uint64_t address, const std::function<void(uint64_t)>& cb_free)
        : active(true)
        , address(address)
        , cb_free(cb_free)
    {
        p_client = NimBLEDevice::createClient({address, 1});
    }

    Connection::~Connection()
    {
        if (active) {
            {
                std::lock_guard connection_lock(connection_mutex);
                if (p_client != nullptr) NimBLEDevice::deleteClient(p_client); // this also disconnects
            }

            if (cb_free != nullptr) cb_free(address);
            if (connected) ESP_LOGV(TAG, "Disconnected %llu", address);
        }
    }

    Connection::Connection(Connection&& other) noexcept
        : active(other.active)
        , connected(other.connected)
        , p_client(other.p_client)
        , address(other.address)
        , cb_free(other.cb_free)
    {
        other.active   = false;
        other.p_client = nullptr;
        other.cb_free  = nullptr;
    }

    template <typename T>
    std::optional<T> Connection::readValueT(const NimBLEUUID& service, const NimBLEUUID& characteristic)
    {
        if (p_client == nullptr) return std::nullopt;
        {
            std::lock_guard connection_lock(connection_mutex);
            if (const auto svc = p_client->getService(service); svc != nullptr) {
                if (const auto chr = svc->getCharacteristic(characteristic); chr != nullptr) {
                    return chr->readValue<T>();
                }
            }
            return std::nullopt;
        }
    }

    bool Connection::connect()
    {
        if (!active || p_client == nullptr || connected) return false;
        {
            std::lock_guard connection_lock(connection_mutex);
            p_client->setConnectRetries(MAX_CONNECTION_RETRIES);
            if (p_client->connect(false)) {
                connected = true;
                ESP_LOGV(TAG, "Connected to %llu", address);
                return true;
            }
            ESP_LOGW(TAG, "Connection to %llu failed (%i)", address, p_client->getLastError());
            return false;
        }
    }

    std::optional<std::string> Connection::readString(const NimBLEUUID& service, const NimBLEUUID& characteristic)
    {
        if (p_client == nullptr) return std::nullopt;
        {
            std::lock_guard connection_lock(connection_mutex);
            if (const auto svc = p_client->getService(service); svc != nullptr) {
                if (const auto chr = svc->getCharacteristic(characteristic); chr != nullptr) {
                    return std::string(chr->readValue().c_str());
                }
            }
            return std::nullopt;
        }
    }

    std::optional<std::vector<uint8_t>> Connection::readBytes(const NimBLEUUID& service, const NimBLEUUID& characteristic)
    {
        if (p_client == nullptr) return std::nullopt;
        {
            std::lock_guard connection_lock(connection_mutex);
            if (const auto svc = p_client->getService(service); svc != nullptr) {
                if (const auto chr = svc->getCharacteristic(characteristic); chr != nullptr) {
                    const auto value = chr->readValue();
                    return std::vector(value.begin(), value.end());
                }
            }
            return std::nullopt;
        }
    }

    std::optional<uint16_t> Connection::readUint16(const NimBLEUUID& service, const NimBLEUUID& characteristic)
    {
        return readValueT<uint16_t>(service, characteristic);
    }

    bool Connection::writeValue(const NimBLEUUID& service, const NimBLEUUID& characteristic, const std::vector<uint8_t>& value)
    {
        if (p_client == nullptr) return false;
        {
            std::lock_guard connection_lock(connection_mutex);
            if (const auto svc = p_client->getService(service); svc != nullptr) {
                if (const auto chr = svc->getCharacteristic(characteristic); chr != nullptr) {
                    return chr->writeValue(value);
                }
            }
        }
        return false;
    }
}
