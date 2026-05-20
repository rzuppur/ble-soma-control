#include "ConnectionPool.h"

#include <chrono>
#include <condition_variable>
#include <unordered_set>

static auto TAG = "BSC";

namespace {
    constexpr auto POOL_MAX_CONNECTIONS = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
    constexpr auto POOL_WAIT_TIMEOUT    = std::chrono::seconds(10);

    std::mutex                   pool_mutex{};
    std::mutex                   connect_mutex{};
    std::condition_variable      pool_cv;
    std::unordered_set<uint64_t> active_connections{};

    void freeConnection(const uint64_t address)
    {
        {
            std::lock_guard pool_lock(pool_mutex);
            active_connections.erase(address);
        }
        pool_cv.notify_one();
    }
}

namespace BLESomaControl::Internal {
    std::optional<Connection> getConnection(const uint64_t address)
    {
        {
            std::unique_lock pool_lock(pool_mutex);
            const auto       success = pool_cv.wait_for(pool_lock, POOL_WAIT_TIMEOUT, [&] {
                return active_connections.size() < POOL_MAX_CONNECTIONS && !active_connections.contains(address);
            });
            if (!success) {
                ESP_LOGW(TAG, "Connection pool timeout to %llu", address);
                return std::nullopt;
            }
            active_connections.insert(address);
        }
        auto connection = Connection(address, freeConnection);
        {
            std::lock_guard connect_lock(connect_mutex);
            ESP_LOGV(TAG, "Connecting.. %llu", address);
            if (connection.connect()) return connection;
        }
        return std::nullopt;
    }

    bool connectionActive()
    {
        std::unique_lock pool_lock(pool_mutex);
        return !active_connections.empty();
    }
}
