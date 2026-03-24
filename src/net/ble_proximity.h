#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace smouse {

// BLE proximity states based on RSSI
enum class ProximityState {
    UNKNOWN,
    NEAR,       // RSSI > -60 dBm (~1-2m)
    MEDIUM,     // RSSI -60 to -80 dBm
    FAR,        // RSSI < -80 dBm (~5m+)
    LOST,       // No BLE signal
};

// BLE proximity detector using Bluetooth Low Energy RSSI
class BleProximity {
public:
    using ProximityCallback = std::function<void(ProximityState state, int rssi)>;

    virtual ~BleProximity() = default;

    // Start advertising as a server (makes this device discoverable)
    virtual bool start_advertising(const std::string& device_name) = 0;

    // Start scanning for a server
    virtual bool start_scanning(ProximityCallback callback) = 0;

    // Stop advertising/scanning
    virtual void stop() = 0;

    // Check if BLE is available on this device
    virtual bool is_available() const = 0;

    // Get current proximity state
    virtual ProximityState current_state() const = 0;

    // RSSI thresholds (configurable)
    static constexpr int RSSI_NEAR = -60;
    static constexpr int RSSI_FAR = -80;
};

// Factory function (implemented per-platform, returns nullptr if BLE not available)
std::unique_ptr<BleProximity> create_ble_proximity();

} // namespace smouse
