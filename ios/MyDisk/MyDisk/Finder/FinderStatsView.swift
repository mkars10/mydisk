import AccessorySetupKit
import SwiftUI
import MyDiskKit

// MyDiskKit test page, kept apart from the rest of the app. Every DiscFinder
// function and value on one screen, labelled with its API name.

extension DiscFinder {
    /// The app's one finder. Bluetooth doesn't exist in the simulator, so it gets
    /// the mock finder there (three fake discs, see DiscFinder.mockEPCs).
    static let shared: DiscFinder = {
        #if targetEnvironment(simulator)
        return .mock()
        #else
        return .bluetooth()
        #endif
    }()
}

struct FinderStatsView: View {
    @State private var finder = DiscFinder.shared
    private let setup = FinderSetup.shared
    @State private var power = 20
    @State private var updateCount = 0
    @State private var disks: [Disk] = []

    var body: some View {
        NavigationStack {
            List {
                Section("AccessorySetupKit") {
                    LabeledContent("accessory", value: setup.accessory?.displayName ?? "none")
                    if let id = setup.accessory?.bluetoothIdentifier {
                        Text(id.uuidString).font(.caption.monospaced()).textSelection(.enabled)
                    }
                    LabeledContent("last event", value: setup.lastEvent)
                    HStack {
                        Button("Set up finder…") { setup.showPicker() }
                        Button("Remove finder") { setup.removeFinder() }
                            .disabled(setup.accessory == nil)
                    }
                    .buttonStyle(.bordered)
                }

                Section("Connection") {
                    LabeledContent("connectionState", value: String(describing: finder.connectionState))
                    HStack {
                        Button("connect()") { finder.connect() }
                        Button("disconnect()") { finder.disconnect() }
                        Button("forgetDevice()") { finder.forgetDevice() }
                    }
                    .buttonStyle(.bordered)
                    .font(.caption)
                }

                Section("Scanning") {
                    LabeledContent("isScanning", value: String(finder.isScanning))
                    LabeledContent("txPower", value: finder.txPower.map { "\($0) dBm" } ?? "unknown")
                    HStack {
                        Button("startScan()") { finder.startScan() }
                        Button("stopScan()") { finder.stopScan() }
                    }
                    .buttonStyle(.bordered)
                    Stepper("setPower(\(power))", value: $power, in: DiscFinder.powerRange)
                        .onChange(of: power) { finder.setPower(power) }
                }

                Section("Stash discs · proximity(of:) / sighting(of:)") {
                    ForEach(disks) { disk in
                        if let epc = disk.epc {
                            FinderTagRow(title: "\(disk.name) · proximity(of:) = \(finder.proximity(of: epc))",
                                         epc: epc, sighting: finder.sighting(of: epc))
                        } else {
                            LabeledContent(disk.name, value: "no epc")
                        }
                    }
                }

                Section("tags (\(finder.tags.count)) · nearbyTags (\(finder.nearbyTags.count))") {
                    if finder.tags.isEmpty {
                        Text("No tags heard since startScan()").foregroundStyle(.secondary)
                    }
                    ForEach(finder.tags) { tag in
                        FinderTagRow(title: nil, epc: tag.epc, sighting: tag)
                    }
                }

                Section("onTagUpdate") {
                    LabeledContent("updates received", value: "\(updateCount)")
                }
            }
            .navigationTitle("Finder Stats")
            .onAppear {
                disks = DiskStore.load()
                finder.onTagUpdate = { _ in updateCount += 1 }
            }
        }
    }
}

/// One tag's latest reading. The EPC is selectable, to copy a new disc's tag ID.
private struct FinderTagRow: View {
    let title: String?
    let epc: String
    let sighting: TagSighting?

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let title { Text(title).font(.headline) }
            Text(epc).font(.caption.monospaced()).textSelection(.enabled)
            if let sighting {
                ProgressView(value: Double(sighting.proximity), total: 100)
                HStack {
                    Text("proximity \(sighting.proximity)")
                    Spacer()
                    Text("rssi \(sighting.rssi) dBm")
                    Spacer()
                    Text("\(sighting.readsPerSecond) reads/s")
                }
                .font(.caption.monospacedDigit())
                TimelineView(.periodic(from: .now, by: 0.5)) { context in
                    let age = context.date.timeIntervalSince(sighting.updatedAt)
                    Text("updatedAt \(age, specifier: "%.1f") s ago · isInRange \(String(sighting.isInRange))")
                }
                .font(.caption2)
                .foregroundStyle(.secondary)
            } else {
                Text("not heard since startScan()").font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

#Preview {
    FinderStatsView()
}
