#import "net/ble_proximity.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import <atomic>
#import <string>
#import <thread>

// S-Mouse BLE service UUID
static NSString* const kSMouseServiceUUID = @"A1B2C3D4-E5F6-7890-ABCD-EF1234567890";

namespace smouse {

class MacOSBleProximity : public BleProximity {
public:
    ~MacOSBleProximity() override {
        stop();
    }

    bool start_advertising(const std::string& device_name) override {
        @autoreleasepool {
            name_ = [NSString stringWithUTF8String:device_name.c_str()];
            mode_ = Mode::ADVERTISER;

            peripheral_mgr_ = [[CBPeripheralManager alloc]
                initWithDelegate:create_peripheral_delegate()
                queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];

            return true;
        }
    }

    bool start_scanning(ProximityCallback callback) override {
        @autoreleasepool {
            callback_ = std::move(callback);
            mode_ = Mode::SCANNER;

            central_mgr_ = [[CBCentralManager alloc]
                initWithDelegate:create_central_delegate()
                queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];

            return true;
        }
    }

    void stop() override {
        if (peripheral_mgr_) {
            [peripheral_mgr_ stopAdvertising];
            peripheral_mgr_ = nil;
        }
        if (central_mgr_) {
            [central_mgr_ stopScan];
            central_mgr_ = nil;
        }
        state_ = ProximityState::UNKNOWN;
    }

    bool is_available() const override {
        if (central_mgr_) {
            return central_mgr_.state == CBManagerStatePoweredOn;
        }
        if (peripheral_mgr_) {
            return peripheral_mgr_.state == CBManagerStatePoweredOn;
        }
        return false;
    }

    ProximityState current_state() const override {
        return state_;
    }

    // Called from delegate when RSSI is received
    void on_rssi(int rssi) {
        ProximityState new_state;
        if (rssi > RSSI_NEAR) {
            new_state = ProximityState::NEAR;
        } else if (rssi > RSSI_FAR) {
            new_state = ProximityState::MEDIUM;
        } else {
            new_state = ProximityState::FAR;
        }

        state_ = new_state;
        if (callback_) {
            callback_(new_state, rssi);
        }
    }

    void on_ble_powered_on() {
        if (mode_ == Mode::ADVERTISER && peripheral_mgr_) {
            CBUUID* serviceUUID = [CBUUID UUIDWithString:kSMouseServiceUUID];
            [peripheral_mgr_ startAdvertising:@{
                CBAdvertisementDataServiceUUIDsKey: @[serviceUUID],
                CBAdvertisementDataLocalNameKey: name_
            }];
        } else if (mode_ == Mode::SCANNER && central_mgr_) {
            CBUUID* serviceUUID = [CBUUID UUIDWithString:kSMouseServiceUUID];
            [central_mgr_ scanForPeripheralsWithServices:@[serviceUUID]
                options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @YES}];
        }
    }

private:
    enum class Mode { NONE, ADVERTISER, SCANNER };

    // Delegate creation is handled through ObjC helper below
    id<CBPeripheralManagerDelegate> create_peripheral_delegate();
    id<CBCentralManagerDelegate> create_central_delegate();

    CBPeripheralManager* peripheral_mgr_ = nil;
    CBCentralManager* central_mgr_ = nil;
    NSString* name_ = nil;
    Mode mode_ = Mode::NONE;
    ProximityCallback callback_;
    std::atomic<ProximityState> state_{ProximityState::UNKNOWN};
};

} // namespace smouse

// ---------- ObjC Delegates ----------

@interface SMBlePeripheralDelegate : NSObject <CBPeripheralManagerDelegate>
@property (nonatomic, assign) smouse::MacOSBleProximity* proximity;
@end

@implementation SMBlePeripheralDelegate
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager*)peripheral {
    if (peripheral.state == CBManagerStatePoweredOn && self.proximity) {
        self.proximity->on_ble_powered_on();
    }
}
@end

@interface SMBleCentralDelegate : NSObject <CBCentralManagerDelegate>
@property (nonatomic, assign) smouse::MacOSBleProximity* proximity;
@end

@implementation SMBleCentralDelegate
- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
    if (central.state == CBManagerStatePoweredOn && self.proximity) {
        self.proximity->on_ble_powered_on();
    }
}

- (void)centralManager:(CBCentralManager*)central
    didDiscoverPeripheral:(CBPeripheral*)peripheral
    advertisementData:(NSDictionary<NSString*, id>*)advertisementData
    RSSI:(NSNumber*)RSSI {
    if (self.proximity) {
        self.proximity->on_rssi([RSSI intValue]);
    }
}
@end

namespace smouse {

id<CBPeripheralManagerDelegate> MacOSBleProximity::create_peripheral_delegate() {
    SMBlePeripheralDelegate* delegate = [[SMBlePeripheralDelegate alloc] init];
    delegate.proximity = this;
    return delegate;
}

id<CBCentralManagerDelegate> MacOSBleProximity::create_central_delegate() {
    SMBleCentralDelegate* delegate = [[SMBleCentralDelegate alloc] init];
    delegate.proximity = this;
    return delegate;
}

std::unique_ptr<BleProximity> create_ble_proximity() {
    return std::make_unique<MacOSBleProximity>();
}

} // namespace smouse
