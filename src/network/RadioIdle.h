#pragma once

#include <cstdint>

// PowerConfig::radioOffWhenIdle (BoardConfig::powerPolicy(); Silkscreen): a
// network screen switches the radio off (or to modem sleep where a later step
// still needs the connection) once its work has ended, instead of when the user
// leaves, and spins the main loop or holds auto-sleep off only while that work
// runs. Always false on other boards and in the simulator, whose HAL has no
// power policy, so their screens keep their behaviour.
namespace radioidle {

bool enabled();

// With enabled(), switch Wi-Fi off now if it is on, and return true when it did.
// A screen that started from the minimal network boot still restarts on exit to
// clear the heap the radio left.
bool release(const char* tag);

// With enabled(), the longest a Nearby screen keeps ESP-NOW listening for, or
// discovering, a peer before it switches the radio off.
constexpr uint32_t NEARBY_WAIT_MS = 2UL * 60UL * 1000UL;
// With enabled(), how long a Nearby result keeps ESP-NOW on to answer a peer
// that missed the last packet and sends again.
constexpr uint32_t NEARBY_RESULT_LINGER_MS = 5000;

}  // namespace radioidle
