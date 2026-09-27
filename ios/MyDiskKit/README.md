# MyDiskKit

Everything the MyDisk app needs to talk to the finder, as one Swift type:
`DiscFinder`. The app never touches Bluetooth. A mock finder lets you build
the UI without hardware.

Requires iOS 17 (uses `@Observable`).

## What you get

```swift
let finder = DiscFinder.bluetooth()   // the real finder
let finder = DiscFinder.mock()        // three fake discs, no hardware

// Connection
finder.connect()          // connects to the last finder, or the first one found; auto-reconnects
finder.connect(to: id)    // connects to one specific finder (e.g. picked in AccessorySetupKit)
finder.disconnect()
finder.forgetDevice()     // next connect() looks for any finder
finder.connectionState    // .bluetoothUnavailable / .disconnected / .searching / .connected

// Scanning
finder.startScan()        // also clears the tag list
finder.stopScan()
finder.isScanning
finder.setPower(15)       // dBm, 5...26 (DiscFinder.powerRange)
finder.txPower

// Tags
finder.proximity(of: epc) // 0-100 for one disc, 0 if not heard
finder.sighting(of: epc)  // full details: proximity, rssi, readsPerSecond, updatedAt
finder.tags               // every tag heard since startScan(), strongest first
finder.nearbyTags         // only the ones heard right now
finder.onTagUpdate = { tag in ... }   // every update, a steady ~5/s per tag

DiscFinder.mockEPCs       // the EPCs the mock reports
```

Every property is observable, so SwiftUI views refresh by themselves.

A tag's EPC is its ID, like `E28068900000500E88C6A4A7`. To find a specific disc,
the app needs to store the EPC on each disc (the `Disk` struct doesn't have one
yet). `finder.nearbyTags` while holding a disc next to the finder is an easy way
to learn a new disc's EPC.

## Add it to the app

1. In Xcode, open `ios/MyDisk/MyDisk.xcodeproj`.
2. **File › Add Package Dependencies… › Add Local…**, choose `ios/MyDiskKit`,
   and add the `MyDiskKit` library to the MyDisk target.
3. In the MyDisk target's **Info** tab, add
   `Privacy - Bluetooth Always Usage Description` (`NSBluetoothAlwaysUsageDescription`)
   with text like "MyDisk uses Bluetooth to connect to your disc finder."
   Without it the app crashes the first time it touches Bluetooth.
4. Optional, for reconnecting while the app is in the background:
   **Signing & Capabilities › + Capability › Background Modes**, tick
   **Uses Bluetooth LE accessories**.

## Smallest working screen

```swift
import SwiftUI
import MyDiskKit

struct FinderDemoView: View {
    @State private var finder = DiscFinder.mock()   // swap for .bluetooth() on a real phone

    var body: some View {
        List {
            Section {
                Text("Connection: \(String(describing: finder.connectionState))")
                Button(finder.isScanning ? "Stop" : "Start") {
                    finder.isScanning ? finder.stopScan() : finder.startScan()
                }
                .disabled(finder.connectionState != .connected)
            }
            Section("Tags") {
                ForEach(finder.tags) { tag in
                    HStack {
                        Text(tag.epc.suffix(6)).monospaced()
                        ProgressView(value: Double(tag.proximity), total: 100)
                        Text("\(tag.proximity)")
                    }
                }
            }
        }
        .onAppear { finder.connect() }
    }
}
```

## How it fits the current app

`ContentView` reads a 0–100 value from a `SignalStrengthSource`, and
`SignalStrengthModel.record()` plays a ping on every call. Two things matter
when plugging the finder in:

- **Connect first.** Call `finder.connect()` once, e.g. when the app starts.
  Commands sent while disconnected are dropped, so `startScan()` does nothing
  until `connectionState == .connected`.
- **Pace the pings yourself.** Tag updates arrive at a steady ~5 a second
  whether the disc is near or far, so pinging on each update just buzzes.
  Poll `proximity(of:)` and sleep less the closer the disc is.

One way to do it, without changing the UI:

```swift
import MyDiskKit

@MainActor
struct FinderSignalStrengthSource: SignalStrengthSource {
    let finder: DiscFinder   // already connecting: finder.connect() at app start
    let epc: String          // the selected disc's tag

    func start(updating model: SignalStrengthModel) async {
        defer { finder.stopScan() }
        while !Task.isCancelled {
            guard finder.connectionState == .connected else {
                model.reset()   // readings freeze while disconnected; don't show them
                try? await Task.sleep(for: .milliseconds(200))
                continue
            }
            // The finder stops scanning when the link drops; start again after a reconnect.
            if !finder.isScanning { finder.startScan() }

            let proximity = finder.proximity(of: epc)
            if proximity > 0 { model.record(Double(proximity)) } else { model.reset() }

            // Geiger pacing: 1 ping a second when faint, 10 a second when on top of it.
            try? await Task.sleep(for: .milliseconds(1000 - proximity * 9))
        }
    }
}
```

`Disk` also needs the tag's ID, as an optional so existing saved stashes
still decode: `var epc: String?`. (`DiskStore.load()` replaces the stash with
mock data if decoding fails.)

## AccessorySetupKit (optional, iOS 18+)

Instead of connecting to the first finder in range, the app can show Apple's
accessory picker and connect to the one the user chose. The finder already
advertises the service UUID the picker needs.

1. Info.plist:
   - `NSAccessorySetupKitSupports`: array with `Bluetooth`
   - `NSAccessorySetupBluetoothServices`: array with
     `576097DE-0001-4B1F-9097-0D8EE45A7F07`
2. Show the picker, then hand the chosen finder to `DiscFinder`:

```swift
import AccessorySetupKit
import CoreBluetooth
import MyDiskKit
import UIKit

@MainActor
final class FinderSetup {
    private let session = ASAccessorySession()
    private let finder: DiscFinder

    init(finder: DiscFinder) {
        self.finder = finder
        session.activate(on: .main) { [weak self] event in
            MainActor.assumeIsolated { self?.handle(event) }
        }
    }

    /// Call from a "Set up finder" button.
    func showPicker(productImage: UIImage) {
        let descriptor = ASDiscoveryDescriptor()
        descriptor.bluetoothServiceUUID = CBUUID(string: "576097DE-0001-4B1F-9097-0D8EE45A7F07")
        let item = ASPickerDisplayItem(name: "MyDisk finder", productImage: productImage,
                                       descriptor: descriptor)
        session.showPicker(for: [item]) { error in
            if let error { print("Picker failed: \(error)") }
        }
    }

    /// Unpairs the finder in iOS and in MyDiskKit.
    func removeFinder() {
        guard let accessory = session.accessories.first else { return }
        session.removeAccessory(accessory) { _ in }
        finder.disconnect()
        finder.forgetDevice()
    }

    private func handle(_ event: ASAccessoryEvent) {
        switch event.eventType {
        case .activated:        // app launch: reconnect to a finder set up earlier
            if let id = session.accessories.first?.bluetoothIdentifier { finder.connect(to: id) }
        case .accessoryAdded:   // the user just picked one
            if let id = event.accessory?.bluetoothIdentifier { finder.connect(to: id) }
        default:
            break
        }
    }
}
```

AccessorySetupKit needs a real iPhone; the picker doesn't work in the simulator.

These are suggestions; restructure them however suits the app.

## Test it

- **Unit tests (Mac, no hardware):** `cd ios/MyDiskKit && swift test`, or open
  the package in Xcode and press ⌘U. Covers the byte formats and the mock.
- **With the mock:** run the demo screen above in the simulator. Three tags
  appear after Start; the first one fades out of range and back every so often.
- **With the finder:** Bluetooth doesn't work in the simulator; use a real
  iPhone. Flash the firmware, switch to `.bluetooth()`, allow Bluetooth when
  asked, and hold a tag near the antenna after Start.

## Files

| File | Job |
|---|---|
| `DiscFinder.swift` | The public API. The only type the app uses |
| `Models.swift` | `TagSighting`, `ConnectionState` |
| `BluetoothTransport.swift` | Core Bluetooth: scan, connect, reconnect, subscribe |
| `MockTransport.swift` | The fake finder |
| `Packets.swift` | Byte layouts from `docs/ble-interface.md` |

## Limitations

- Connects to one finder; the first one found if none is remembered.
- No state restoration: if iOS terminates the app in the background, it won't
  be relaunched on reconnect.
- Commands sent while disconnected are dropped.
- Tested against the real finder and with `swift test`, but not yet run
  inside the MyDisk app.
