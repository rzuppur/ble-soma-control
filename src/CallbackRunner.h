#pragma once

#include "BLESomaControl.h"

namespace BLESomaControl::Internal::CallbackRunner {
    enum class Type { FOUND, UPDATE };
    struct Item {
        uint64_t address;
        Type     type;
    };

    bool begin(DeviceCallback found, DeviceCallback update);
    bool end();
    bool queueCallback(const Item& item);
}
