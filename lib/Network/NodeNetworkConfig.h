#pragma once

#include <Arduino.h>

namespace NodeNetworkConfig
{
  // Wire the node's active-low game input between this pin and ground.
  constexpr uint8_t InputButtonPin = 27;

  // Ignore input-button transitions that occur within this debounce interval.
  constexpr uint16_t InputDebounceTimeMs = 50;

  // Holding the game input during boot starts pairing.
  constexpr uint8_t PairingButtonPin = InputButtonPin;

  // Ignore pairing-button transitions that occur within this debounce interval.
  constexpr uint16_t PairingDebounceTimeMs = 50;

  // The ESP32 DevKit's onboard status LED is wired to GPIO 2.
  constexpr uint8_t StatusLedPin = 2;

  // Set the full period of a remote node's status LED heartbeat.
  constexpr uint32_t StatusLedHeartbeatPeriodMs = 1000;

  // Set how long the remote node's status LED remains on during each heartbeat.
  constexpr uint32_t StatusLedHeartbeatOnDurationMs = 100;

  // Set the half-period used while a node is in either pairing state.
  constexpr uint32_t StatusLedPairingFlashPeriodMs = 50;

  // Require this continuous boot-time hold before starting pairing.
  constexpr uint32_t PairingHoldDurationMs = 5000;

  // End a pairing session after this duration.
  constexpr uint32_t PairingWindowDurationMs = 30000;

  // Wait this long for a home advertisement before becoming the home node.
  constexpr uint32_t HomeDiscoveryDurationMs = 1500;

  // Broadcast home advertisements at this interval while accepting remotes.
  constexpr uint32_t HomeAdvertisementIntervalMs = 500;

  // Limit the number of remote nodes a home node accepts in one session.
  constexpr uint8_t MaxRemoteNodes = 10;

  // Limit the payload size of an application event packet.
  constexpr uint8_t MaxEventPayloadBytes = 200;
}
