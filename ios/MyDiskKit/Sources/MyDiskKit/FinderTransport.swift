import Foundation

/// Something that carries commands to a finder and events back.
/// `BluetoothTransport` talks to real hardware; `MockTransport` makes things up.
/// `DiscFinder` only ever sees this protocol.
@MainActor
protocol FinderTransport: AnyObject {
    var onEvent: ((TransportEvent) -> Void)? { get set }
    func connect()
    func connect(to id: UUID)
    func disconnect()
    func forgetDevice()
    func send(_ command: FinderCommand)
}

enum TransportEvent {
    case connection(ConnectionState)
    case status(DeviceStatus)
    case tag(TagSighting)
}
