# MyDisk Bluetooth interface (v1, prototype)

The contract between the finder firmware (`firmware/DiscTracker/ble_link.cpp`)
and the iOS package (`ios/MyDiskKit`). The app itself never sees these bytes;
it uses `DiscFinder` from MyDiskKit. Change both sides together.

Decided by Tanner on 2026-09-26: WiFi and the web page are gone, the
"hotter/colder" maths runs on the ESP32, and Tanner owns the Swift package.
Prototype defaults (roadmap recommendations): built-in ESP32 BLE library, text
commands and binary readings, no pairing, scanning stops when the phone
disconnects.

## Device

| Item | Value |
|---|---|
| Transport | Bluetooth Low Energy (GATT), ESP32 is the peripheral |
| Advertised name | `MyDisk` |
| Advertised service | the service UUID below, so iOS can find it by UUID, including in the background |
| Connections | one phone at a time |
| Pairing | none |
| Advertising | whenever no phone is connected |
| On disconnect | scanning stops, so the transmitter never runs unattended |

## Service and characteristics

Service UUID: `576097de-0001-4b1f-9097-0d8ee45a7f07`

| Characteristic | UUID | Properties | Direction |
|---|---|---|---|
| Command | `576097de-0002-4b1f-9097-0d8ee45a7f07` | Write | phone → finder |
| Tag update | `576097de-0003-4b1f-9097-0d8ee45a7f07` | Notify | finder → phone |
| Status | `576097de-0004-4b1f-9097-0d8ee45a7f07` | Read, Notify | finder → phone |

### Command (write, ASCII text)

The same commands as the USB serial port, no newline.

| Command | Meaning |
|---|---|
| `S` | Start scanning |
| `X` | Stop scanning |
| `P<dBm>` | Set transmit power, e.g. `P20`. Accepted 5–26 |

Invalid commands are ignored. The result shows up in Status.

### Tag update (notify, binary)

Every 200 ms, one notification per tag heard in the last 3 s. Proximity falls
to 0 two seconds after the last read, so the phone sees a tag fade out rather
than freeze, then updates for it stop.

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | `rssi` | i8 | smoothed signal strength, dBm, e.g. -57 |
| 1 | `proximity` | u8 | 0–100 "hotter/colder", computed on the finder |
| 2 | `readsPerSec` | u8 | how often the reader hears the tag |
| 3 | `epcLen` | u8 | EPC bytes that follow (usually 12, at most 16) |
| 4… | `epc` | bytes | tag ID; shown as uppercase hex |

At most 20 bytes, which fits the default BLE packet with no negotiation.

Example: `CE 3D 0B 0C E2 80 68 90 00 00 50 0E 88 C6 A4 DD` is RSSI −50,
proximity 61, 11 reads/s, EPC `E28068900000500E88C6A4DD`.

### Status (read + notify, binary)

Read once on connect, then notified whenever it changes.

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | `version` | u8 | protocol version, `1` |
| 1 | `scanning` | u8 | 1 = scanning, 0 = idle |
| 2 | `txPower` | u8 | transmit power, dBm |

## Limitations of v1

- One phone at a time.
- No pairing or encryption: anyone nearby with a BLE app can connect and start
  the transmitter.
- No battery level (the prototype can't measure it).
- No filtering on the finder: it reports every tag; the app picks its disc.
- No device ID or firmware version characteristic.
- Maths tuning (`firmware/DiscTracker/tracker.h`) needs a reflash to change.

## Changing this contract

Bump `version` in Status when any layout changes, and update
`ble_link.cpp`, `ios/MyDiskKit/Sources/MyDiskKit/Packets.swift` and this file
in the same change.
