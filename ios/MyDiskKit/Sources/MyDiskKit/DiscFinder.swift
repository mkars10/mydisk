import Foundation
import Observation

/// The whole finder, as the app sees it. The only type the UI needs.
///
///     @State private var finder = DiscFinder.bluetooth()   // or .mock()
///
///     finder.connect()
///     finder.startScan()
///     finder.proximity(of: disc.epc)   // 0-100, drives the bar and the clicks
///
/// Every property is observable, so SwiftUI views update on their own.
@MainActor
@Observable
public final class DiscFinder {
    /// Transmit power range the finder accepts, in dBm.
    public static let powerRange = 5...26

    public private(set) var connectionState: ConnectionState = .disconnected
    /// True while the finder's reader is transmitting and reading tags.
    public private(set) var isScanning = false
    /// Current transmit power in dBm, once the finder has reported it.
    public private(set) var txPower: Int?
    /// Every tag heard since the last `startScan()`, strongest first.
    /// Tags that go out of range stay in the list with proximity 0.
    public private(set) var tags: [TagSighting] = []

    /// Tags being heard right now, strongest first.
    public var nearbyTags: [TagSighting] { tags.filter(\.isInRange) }

    /// Called for every tag update (about 5 a second per tag in range, at a steady
    /// rate however close the tag is). For display, observing `tags` is enough.
    @ObservationIgnored public var onTagUpdate: ((TagSighting) -> Void)?

    private let transport: FinderTransport
    @ObservationIgnored private var tagsByEPC: [String: TagSighting] = [:]

    init(transport: FinderTransport) {
        self.transport = transport
        transport.onEvent = { [weak self] event in self?.handle(event) }
    }

    /// A finder over Bluetooth. Needs `NSBluetoothAlwaysUsageDescription` in the app's Info.plist.
    public static func bluetooth() -> DiscFinder { DiscFinder(transport: BluetoothTransport()) }

    /// A pretend finder with three fake discs, for building UI without hardware.
    public static func mock() -> DiscFinder { DiscFinder(transport: MockTransport()) }

    /// The EPCs the mock finder reports. Put these on mock discs so they can be "found".
    public static var mockEPCs: [String] { MockTransport.fakeEPCs }

    // MARK: - Connection

    /// Connects to the finder used last time, or the first one found if none is remembered.
    /// If the finder is off or out of range, this keeps waiting and connects when it appears,
    /// and reconnects by itself after a drop, until `disconnect()` is called.
    public func connect() { transport.connect() }

    /// Connects to one specific finder and remembers it, e.g. the one the user picked in
    /// AccessorySetupKit (`ASAccessory.bluetoothIdentifier`). Otherwise behaves like `connect()`.
    public func connect(to finderID: UUID) { transport.connect(to: finderID) }

    /// Disconnects and stops trying. The finder stops scanning when the phone disconnects.
    public func disconnect() { transport.disconnect() }

    /// Forgets the remembered finder so the next `connect()` looks for any finder.
    public func forgetDevice() { transport.forgetDevice() }

    // MARK: - Scanning

    /// Starts the reader and clears the tag list.
    public func startScan() {
        tagsByEPC.removeAll()
        tags = []
        transport.send(.startScan)
    }

    public func stopScan() { transport.send(.stopScan) }

    /// Sets transmit power in dBm, clamped to `powerRange`. Lower power gives a
    /// better gradient up close; higher power reaches farther.
    public func setPower(_ dBm: Int) {
        transport.send(.setPower(min(max(dBm, Self.powerRange.lowerBound), Self.powerRange.upperBound)))
    }

    // MARK: - Reading tags

    /// 0-100 for the tag with this EPC (case-insensitive), or 0 if it isn't being heard.
    public func proximity(of epc: String) -> Int {
        tagsByEPC[epc.uppercased()]?.proximity ?? 0
    }

    /// Latest update for the tag with this EPC, if it has been heard since `startScan()`.
    public func sighting(of epc: String) -> TagSighting? {
        tagsByEPC[epc.uppercased()]
    }

    // MARK: - Events from the transport

    private func handle(_ event: TransportEvent) {
        switch event {
        case .connection(let state):
            connectionState = state
            if state != .connected { isScanning = false }
        case .status(let status):
            isScanning = status.isScanning
            txPower = status.txPower
        case .tag(let sighting):
            tagsByEPC[sighting.epc] = sighting
            tags = tagsByEPC.values.sorted {
                $0.proximity != $1.proximity ? $0.proximity > $1.proximity : $0.epc < $1.epc
            }
            onTagUpdate?(sighting)
        }
    }
}
