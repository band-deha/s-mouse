// BLE proximity - platform-specific implementations:
// - macOS: Uses CoreBluetooth (see src/platform/macos/macos_ble.mm)
// - Windows: Uses WinRT Bluetooth LE APIs (see src/platform/windows/win_ble.cpp)
//
// This file provides a fallback for platforms without BLE support.

#include "ble_proximity.h"

namespace smouse {

// Default implementation for platforms without BLE
class NullBleProximity : public BleProximity {
public:
    bool start_advertising(const std::string& /*device_name*/) override { return false; }
    bool start_scanning(ProximityCallback /*callback*/) override { return false; }
    void stop() override {}
    bool is_available() const override { return false; }
    ProximityState current_state() const override { return ProximityState::UNKNOWN; }
};

// Platform-specific factory will override this via weak linkage or conditional compilation
#if !defined(__APPLE__) && !defined(_WIN32)
std::unique_ptr<BleProximity> create_ble_proximity() {
    return std::make_unique<NullBleProximity>();
}
#endif

} // namespace smouse
