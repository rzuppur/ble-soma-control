#pragma once
#include <optional>

#include "Connection.h"

namespace BLESomaControl::Internal {
    std::optional<Connection> getConnection(uint64_t address);
    bool                      connectionActive();
}
