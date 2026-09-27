import AccessorySetupKit
import CoreBluetooth
import MyDiskKit
import Observation
import UIKit

/// AccessorySetupKit prototype. Apple's picker chooses the finder; MyDiskKit then
/// connects to it with connect(to:). The picker needs a real iPhone (iOS 18+).
@Observable
final class FinderSetup {
    /// The finder this app has been given access to, if any.
    private(set) var accessory: ASAccessory?
    /// Latest session event, for the stats page.
    private(set) var lastEvent = "not activated"

    @ObservationIgnored private let session = ASAccessorySession()
    @ObservationIgnored private let finder: DiscFinder

    init(finder: DiscFinder) {
        self.finder = finder
        session.activate(on: .main) { [weak self] event in
            MainActor.assumeIsolated { self?.handle(event) }
        }
    }

    /// Shows Apple's accessory sheet, listing MyDisk finders that are advertising.
    func showPicker() {
        // The picker refuses to open while the app has a Bluetooth manager running with
        // global Bluetooth permission, so drop any current connection first.
        finder.disconnect()
        let descriptor = ASDiscoveryDescriptor()
        descriptor.bluetoothServiceUUID = CBUUID(string: "576097DE-0001-4B1F-9097-0D8EE45A7F07")
        let item = ASPickerDisplayItem(name: "MyDisk finder", productImage: Self.productImage,
                                       descriptor: descriptor)
        session.showPicker(for: [item]) { [weak self] error in
            guard let error else { return }
            MainActor.assumeIsolated { self?.lastEvent = "picker error: \(error.localizedDescription)" }
        }
    }

    /// Revokes the app's access to the finder (same as removing it in iOS Settings).
    func removeFinder() {
        guard let accessory else { return }
        session.removeAccessory(accessory) { [weak self] error in
            guard let error else { return }
            MainActor.assumeIsolated { self?.lastEvent = "remove error: \(error.localizedDescription)" }
        }
    }

    private func handle(_ event: ASAccessoryEvent) {
        switch event.eventType {
        case .activated:          // app launch: reconnect to a finder set up earlier
            lastEvent = "activated"
            accessory = session.accessories.first
            if let id = accessory?.bluetoothIdentifier { finder.connect(to: id) }
        case .accessoryAdded:     // the user picked one in the sheet
            lastEvent = "accessoryAdded"
            accessory = event.accessory
            if let id = event.accessory?.bluetoothIdentifier { finder.connect(to: id) }
        case .accessoryRemoved:   // removed here or in iOS Settings
            lastEvent = "accessoryRemoved"
            accessory = nil
            finder.disconnect()
            finder.forgetDevice()
        case .pickerDidPresent:   lastEvent = "pickerDidPresent"
        case .pickerDidDismiss:   lastEvent = "pickerDidDismiss"
        case .pickerSetupFailed:  lastEvent = "pickerSetupFailed"
        case .invalidated:        lastEvent = "invalidated"
        default:                  lastEvent = "event \(event.eventType.rawValue)"
        }
    }

    /// Placeholder picture for the sheet until there's a photo of the finder.
    private static let productImage: UIImage = {
        let symbol = UIImage(systemName: "dot.radiowaves.left.and.right",
                             withConfiguration: UIImage.SymbolConfiguration(pointSize: 120))!
            .withTintColor(.systemBlue)
        let side: CGFloat = 200
        return UIGraphicsImageRenderer(size: CGSize(width: side, height: side)).image { _ in
            let size = symbol.size
            symbol.draw(in: CGRect(x: (side - size.width) / 2, y: (side - size.height) / 2,
                                   width: size.width, height: size.height))
        }
    }()
}

extension FinderSetup {
    /// One session for the whole app, driving the shared finder.
    static let shared = FinderSetup(finder: .shared)
}
