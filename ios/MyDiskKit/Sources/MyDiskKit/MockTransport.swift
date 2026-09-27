import Foundation

/// A pretend finder for building and previewing UI without hardware.
/// Connects after half a second and, while scanning, reports three fake discs
/// whose proximity drifts up and down. The first one fades in and out of range.
@MainActor
final class MockTransport: FinderTransport {
    /// EPCs the mock reports, so the app can put them on its mock discs.
    static let fakeEPCs = [
        "E28068900000500E88C6A4A7",
        "E28068900000500E88C6B112",
        "E28068900000500E88C6C3F0",
    ]

    var onEvent: ((TransportEvent) -> Void)?

    private var connected = false
    private var scanning = false
    private var power = 20
    private var loop: Task<Void, Never>?
    private var tick = 0

    func connect() {
        guard !connected else { return }
        onEvent?(.connection(.searching))
        loop?.cancel()
        loop = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(500))
            guard let self, !Task.isCancelled else { return }
            self.connected = true
            self.onEvent?(.connection(.connected))
            self.reportStatus()
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(200))   // same rate as the real finder
                self.step()
            }
        }
    }

    func disconnect() {
        loop?.cancel()
        loop = nil
        connected = false
        scanning = false   // the real finder stops scanning when the phone leaves
        onEvent?(.connection(.disconnected))
    }

    func connect(to id: UUID) { connect() }

    func forgetDevice() {}

    func send(_ command: FinderCommand) {
        guard connected else { return }
        switch command {
        case .startScan: scanning = true
        case .stopScan: scanning = false
        case .setPower(let dBm): power = dBm
        }
        reportStatus()
    }

    private func reportStatus() {
        onEvent?(.status(DeviceStatus(protocolVersion: Packets.protocolVersion, isScanning: scanning, txPower: power)))
    }

    private func step() {
        guard scanning else { return }
        tick += 1
        let t = Double(tick) / 5.0   // seconds
        for (i, epc) in Self.fakeEPCs.enumerated() {
            // Slow waves with different periods, so each disc feels different.
            let wave = 0.5 + 0.5 * sin(t / Double(3 + i * 2) + Double(i))
            var proximity = Int(wave * Double(90 - i * 20))
            if i == 0 && proximity < 15 { proximity = 0 }   // drops out of range at the bottom of its swing
            let sighting = TagSighting(
                epc: epc,
                proximity: proximity,
                rssi: proximity == 0 ? -80 : -80 + proximity * 45 / 100,
                readsPerSecond: proximity / 5
            )
            onEvent?(.tag(sighting))
        }
    }
}
