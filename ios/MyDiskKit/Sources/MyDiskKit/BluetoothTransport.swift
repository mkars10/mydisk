import CoreBluetooth
import Foundation

/// Talks to a real finder over Bluetooth LE using Core Bluetooth.
/// The only file in the package (and the app) that knows about Bluetooth.
@MainActor
final class BluetoothTransport: NSObject, FinderTransport {
    // Keep in sync with docs/ble-interface.md and firmware/DiscTracker/ble_link.cpp.
    static let serviceUUID = CBUUID(string: "576097DE-0001-4B1F-9097-0D8EE45A7F07")
    static let commandUUID = CBUUID(string: "576097DE-0002-4B1F-9097-0D8EE45A7F07")
    static let tagUUID     = CBUUID(string: "576097DE-0003-4B1F-9097-0D8EE45A7F07")
    static let statusUUID  = CBUUID(string: "576097DE-0004-4B1F-9097-0D8EE45A7F07")

    private static let rememberedKey = "MyDiskKit.rememberedFinderID"

    var onEvent: ((TransportEvent) -> Void)?

    /// Created on connect and released on disconnect: AccessorySetupKit won't show its
    /// picker while an app with global Bluetooth permission has a manager alive.
    private var central: CBCentralManager?
    private var peripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?
    /// True between connect() and disconnect(): keep trying and reconnect after drops.
    private var wantsConnection = false

    // MARK: - FinderTransport

    func connect() {
        wantsConnection = true
        if central == nil {
            // Callbacks on the main queue; centralManagerDidUpdateState starts connecting.
            central = CBCentralManager(delegate: self, queue: nil)
            return
        }
        startConnecting()
    }

    func connect(to id: UUID) {
        if let peripheral, peripheral.identifier != id {
            central?.cancelPeripheralConnection(peripheral)   // switching finders
            self.peripheral = nil
            commandCharacteristic = nil
        }
        rememberedID = id
        connect()
    }

    func disconnect() {
        wantsConnection = false
        central?.stopScan()
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        central = nil
        peripheral = nil
        commandCharacteristic = nil
        onEvent?(.connection(.disconnected))
    }

    func forgetDevice() {
        UserDefaults.standard.removeObject(forKey: Self.rememberedKey)
    }

    func send(_ command: FinderCommand) {
        guard let peripheral, let commandCharacteristic else { return }
        peripheral.writeValue(Packets.encode(command), for: commandCharacteristic, type: .withResponse)
    }

    // MARK: - Connecting

    private var rememberedID: UUID? {
        get { UserDefaults.standard.string(forKey: Self.rememberedKey).flatMap(UUID.init(uuidString:)) }
        set { UserDefaults.standard.set(newValue?.uuidString, forKey: Self.rememberedKey) }
    }

    private func startConnecting() {
        guard wantsConnection, let central else { return }
        guard central.state == .poweredOn else {
            // .unknown and .resetting settle by themselves; centralManagerDidUpdateState calls back.
            if central.state != .unknown && central.state != .resetting {
                onEvent?(.connection(.bluetoothUnavailable))
            }
            return
        }
        if let peripheral, peripheral.state != .disconnected { return }

        onEvent?(.connection(.searching))
        if let id = rememberedID, let known = central.retrievePeripherals(withIdentifiers: [id]).first {
            // A connect request to a known device never times out: iOS completes it
            // whenever the finder comes into range, even with the app in the background.
            attach(known)
            central.connect(known)
        } else {
            central.scanForPeripherals(withServices: [Self.serviceUUID])
        }
    }

    private func attach(_ newPeripheral: CBPeripheral) {
        peripheral = newPeripheral
        newPeripheral.delegate = self
    }

    private func linkReady() {
        guard let peripheral else { return }
        rememberedID = peripheral.identifier
        onEvent?(.connection(.connected))
    }

    private func linkLost() {
        commandCharacteristic = nil
        if wantsConnection, let peripheral {
            onEvent?(.connection(.searching))
            central?.connect(peripheral)   // reconnects by itself when back in range
        } else {
            onEvent?(.connection(.disconnected))
        }
    }

    private func handleValue(_ data: Data, from uuid: CBUUID) {
        if uuid == Self.tagUUID, let sighting = Packets.decodeTag(data) {
            onEvent?(.tag(sighting))
        } else if uuid == Self.statusUUID, let status = Packets.decodeStatus(data) {
            onEvent?(.status(status))
        }
    }
}

// MARK: - Core Bluetooth callbacks (delivered on the main queue)

extension BluetoothTransport: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated {
            guard central === self.central else { return }   // a manager released by disconnect()
            if central.state == .poweredOn {
                startConnecting()
            } else if central.state != .unknown && central.state != .resetting {
                peripheral = nil   // Core Bluetooth invalidates peripherals when the radio goes away
                commandCharacteristic = nil
                onEvent?(.connection(.bluetoothUnavailable))
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any], rssi RSSI: NSNumber) {
        MainActor.assumeIsolated {
            central.stopScan()
            attach(peripheral)
            central.connect(peripheral)
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        MainActor.assumeIsolated {
            peripheral.discoverServices([Self.serviceUUID])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        MainActor.assumeIsolated {
            guard peripheral.identifier == self.peripheral?.identifier else { return }   // an old finder
            linkLost()
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        MainActor.assumeIsolated {
            guard peripheral.identifier == self.peripheral?.identifier else { return }   // an old finder
            linkLost()
        }
    }
}

extension BluetoothTransport: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        MainActor.assumeIsolated {
            guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else { return }
            peripheral.discoverCharacteristics([Self.commandUUID, Self.tagUUID, Self.statusUUID], for: service)
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        MainActor.assumeIsolated {
            for characteristic in service.characteristics ?? [] {
                switch characteristic.uuid {
                case Self.commandUUID:
                    commandCharacteristic = characteristic
                case Self.tagUUID:
                    peripheral.setNotifyValue(true, for: characteristic)
                case Self.statusUUID:
                    peripheral.setNotifyValue(true, for: characteristic)
                    peripheral.readValue(for: characteristic)
                default:
                    break
                }
            }
            if commandCharacteristic != nil { linkReady() }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        MainActor.assumeIsolated {
            guard error == nil, let data = characteristic.value else { return }
            handleValue(data, from: characteristic.uuid)
        }
    }
}
