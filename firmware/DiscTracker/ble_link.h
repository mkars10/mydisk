// Bluetooth Low Energy link for the phone app. Implements docs/ble-interface.md.
// Takes commands through commands::handle and reads tag state from the
// tracker; never talks to the reader directly.
#pragma once

namespace ble_link {

// Starts advertising. `onDisconnect` runs on the main loop when the phone
// drops, so the sketch decides what to do (it stops scanning).
void begin(const char *deviceName, void (*onDisconnect)());

// Call every loop(): runs queued commands and sends tag and status updates.
void poll();

bool isConnected();

}  // namespace ble_link
