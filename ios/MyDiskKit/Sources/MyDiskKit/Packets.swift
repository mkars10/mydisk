import Foundation

/// Byte layouts from docs/ble-interface.md. Pure functions, no Bluetooth,
/// so they can be unit tested.
enum Packets {
    static let protocolVersion = 1

    /// Tag reading: rssi(i8) | proximity(u8) | readsPerSec(u8) | epcLen(u8) | epc
    static func decodeTag(_ data: Data, at date: Date = Date()) -> TagSighting? {
        let bytes = [UInt8](data)
        guard bytes.count >= 4 else { return nil }
        let epcLen = Int(bytes[3])
        guard epcLen > 0, bytes.count >= 4 + epcLen else { return nil }
        let epc = bytes[4..<(4 + epcLen)].map { String(format: "%02X", $0) }.joined()
        return TagSighting(
            epc: epc,
            proximity: min(Int(bytes[1]), 100),
            rssi: Int(Int8(bitPattern: bytes[0])),
            readsPerSecond: Int(bytes[2]),
            updatedAt: date
        )
    }

    /// Status: version(u8) | scanning(u8) | txPower(u8)
    static func decodeStatus(_ data: Data) -> DeviceStatus? {
        let bytes = [UInt8](data)
        guard bytes.count >= 3 else { return nil }
        return DeviceStatus(protocolVersion: Int(bytes[0]), isScanning: bytes[1] != 0, txPower: Int(bytes[2]))
    }

    /// Commands are short ASCII text, the same as the USB serial port.
    static func encode(_ command: FinderCommand) -> Data {
        switch command {
        case .startScan: return Data("S".utf8)
        case .stopScan: return Data("X".utf8)
        case .setPower(let dBm): return Data("P\(dBm)".utf8)
        }
    }
}
