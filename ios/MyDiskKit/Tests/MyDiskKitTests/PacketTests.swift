import XCTest
@testable import MyDiskKit

/// Checks the byte layouts in docs/ble-interface.md. The tag bytes are the
/// exact packet the firmware's host test (firmware/host_test) produces.
final class PacketTests: XCTestCase {
    func testDecodesTagReading() throws {
        let bytes: [UInt8] = [0xCE, 0x3D, 0x0B, 0x0C,
                              0xE2, 0x80, 0x68, 0x90, 0x00, 0x00, 0x50, 0x0E, 0x88, 0xC6, 0xA4, 0xDD]
        let tag = try XCTUnwrap(Packets.decodeTag(Data(bytes)))
        XCTAssertEqual(tag.epc, "E28068900000500E88C6A4DD")
        XCTAssertEqual(tag.rssi, -50)
        XCTAssertEqual(tag.proximity, 61)
        XCTAssertEqual(tag.readsPerSecond, 11)
    }

    func testRejectsShortTagReading() {
        XCTAssertNil(Packets.decodeTag(Data([0xCE, 0x3D, 0x0B, 0x0C, 0xE2])))
        XCTAssertNil(Packets.decodeTag(Data([0xCE, 0x3D])))
    }

    func testDecodesStatus() throws {
        let status = try XCTUnwrap(Packets.decodeStatus(Data([0x01, 0x01, 0x0F])))
        XCTAssertEqual(status, DeviceStatus(protocolVersion: 1, isScanning: true, txPower: 15))
    }

    func testEncodesCommands() {
        XCTAssertEqual(Packets.encode(.startScan), Data("S".utf8))
        XCTAssertEqual(Packets.encode(.stopScan), Data("X".utf8))
        XCTAssertEqual(Packets.encode(.setPower(15)), Data("P15".utf8))
    }

    @MainActor
    func testMockFinderReportsTags() async throws {
        let finder = DiscFinder.mock()
        finder.connect()
        try await Task.sleep(for: .milliseconds(700))
        XCTAssertEqual(finder.connectionState, .connected)
        finder.startScan()
        try await Task.sleep(for: .milliseconds(700))
        XCTAssertTrue(finder.isScanning)
        XCTAssertEqual(finder.tags.count, 3)
        finder.disconnect()
        XCTAssertEqual(finder.connectionState, .disconnected)
        XCTAssertFalse(finder.isScanning)
    }
}
