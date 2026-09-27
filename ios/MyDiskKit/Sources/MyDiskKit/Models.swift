import Foundation

/// One tag (disc) the finder can currently hear. Updated about 5 times a second
/// while it's in range. The numbers are computed on the finder itself.
public struct TagSighting: Identifiable, Equatable, Sendable {
    /// The tag's EPC as uppercase hex, e.g. "E28068900000500E88C6A4A7".
    /// Store this on a disc to find it later.
    public let epc: String
    /// 0-100 "hotter/colder" value. 0 means the tag isn't being heard any more.
    public let proximity: Int
    /// Smoothed signal strength in dBm, e.g. -57. Useful for debugging.
    public let rssi: Int
    /// How many times a second the reader is hearing this tag.
    public let readsPerSecond: Int
    /// When this update arrived on the phone.
    public let updatedAt: Date

    public var id: String { epc }
    public var isInRange: Bool { proximity > 0 }

    public init(epc: String, proximity: Int, rssi: Int, readsPerSecond: Int, updatedAt: Date = Date()) {
        self.epc = epc
        self.proximity = proximity
        self.rssi = rssi
        self.readsPerSecond = readsPerSecond
        self.updatedAt = updatedAt
    }
}

/// Whether the phone is talking to a finder.
public enum ConnectionState: Equatable, Sendable {
    /// Bluetooth is off, unsupported, or the user denied permission.
    case bluetoothUnavailable
    /// Not connected and not trying.
    case disconnected
    /// Looking for a finder, or waiting for a remembered one to come into range.
    case searching
    case connected
}

/// What the finder reports about itself.
struct DeviceStatus: Equatable {
    var protocolVersion: Int
    var isScanning: Bool
    var txPower: Int
}

/// Commands the phone can send. Kept internal: the app uses `DiscFinder`'s methods.
enum FinderCommand: Equatable {
    case startScan
    case stopScan
    case setPower(Int)
}
