Unofficial library to control SOMA smart blinds over bluetooth.

## Use

```c++
#include <BLESomaControl.h>

void main {
    BLESomaControl::begin(nullptr, nullptr);
    for (auto& p_device : BLESomaControl::getDevices()) {
        p_device->commandPosition(50);
    }
}
```

## Example

Run the example project for ESP32-C3

```
pio run -t upload -t monitor -e esp32c3
```
