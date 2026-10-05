#pragma once

#include <Arduino.h>
#include <Button2.h>
#include <esp_idf_version.h>
#include <esp_now.h>
#include "NodeNetworkConfig.h"

// Connects one home node to multiple remote nodes over ESP-NOW.
class NodeNetwork
{
 public:
  enum class Role : uint8_t { Unconfigured, Home, Remote };

  // Represents the inputs reported by one node. More inputs can be added here later.
  struct InputState
  {
    InputState(bool local = false, bool pressed = false)
      : isLocal(local), buttonPressed(pressed) {}

    bool isLocal;
    bool buttonPressed;
  };
  using EventHandler = void (*)(const uint8_t sourceMac[6], uint8_t eventType,
                                const uint8_t* payload, uint8_t payloadLength);

  void begin();
  void update();
  void setEventHandler(EventHandler handler) { eventHandler_ = handler; }

  Role role() const { return storedRole_; }
  bool isPairing() const;
  bool isHomeReady() const;
  uint8_t remoteCount() const;
  const InputState& homeInputState() const { return homeInputState_; }
  const InputState* remoteInputState(uint8_t remoteIndex) const;
  bool sendToHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength);
  bool sendToRemote(const uint8_t remoteMac[6], uint8_t eventType,
                    const uint8_t* payload, uint8_t payloadLength);
  void broadcastFromHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength);

 private:
  enum class State : uint8_t { CheckingBootHold, Normal, SeekingHome, HomePairing };
  enum class PacketType : uint8_t { HomeAdvertisement = 1, PairRequest, PairAccepted, Event };

  struct PairingStorage
  {
    uint8_t role;
    uint8_t remoteCount;
    uint8_t homeMac[6];
    struct RemoteNode
    {
      uint8_t mac[6];
      InputState inputState;
    } remoteNodes[NodeNetworkConfig::MaxRemoteNodes];
  };

  struct PacketHeader
  {
    uint8_t marker;
    PacketType type;
  } __attribute__((packed));

  struct EventPacket
  {
    PacketHeader header;
    uint8_t eventType;
    uint8_t payloadLength;
    uint8_t payload[NodeNetworkConfig::MaxEventPayloadBytes];
  } __attribute__((packed));

  struct QueuedPacket
  {
    uint8_t sourceMac[6];
    uint16_t length;
    uint8_t data[250];
  };

  static constexpr uint8_t PacketMarker = 0xB7;
  static constexpr uint8_t ReceiveQueueSize = 8;
  static constexpr uint8_t InputStateEventType = 1;

  PairingStorage storage_ = {};
  InputState homeInputState_ = InputState(true, false);
  InputState remoteLocalInputState_ = InputState(true, false);
  Button2 inputButton_;
  bool debouncedInputPressed_ = false;
  bool remoteInputStateSent_ = false;
  Role storedRole_ = Role::Unconfigured;
  State state_ = State::CheckingBootHold;
  bool bootHoldActive_ = false;
  uint32_t bootCheckStartedAt_ = 0;
  uint32_t pairingStartedAt_ = 0;
  uint32_t lastHomeAdvertisementAt_ = 0;
  uint8_t selectedHomeMac_[6] = {};
  QueuedPacket receiveQueue_[ReceiveQueueSize] = {};
  volatile uint8_t receiveHead_ = 0;
  volatile uint8_t receiveTail_ = 0;
  portMUX_TYPE receiveQueueMutex_ = portMUX_INITIALIZER_UNLOCKED;
  bool started_ = false;
  EventHandler eventHandler_ = nullptr;

  void loadStorage();
  void saveStorage() const;
  void clearPairingStorage();
  void addStoredPeers();
  void startPairing();
  void becomeHome();
  void finishRemotePairing(const uint8_t homeMac[6]);
  void sendHomeAdvertisement();
  void updateLocalInputState();
  void updateRemoteInputState(const uint8_t sourceMac[6], bool buttonPressed);
  static void inputPressedHandler(Button2& button);
  static void inputReleasedHandler(Button2& button);
  void updateStatusLed();
  void handlePacket(const QueuedPacket& packet);
  void handleEvent(const QueuedPacket& packet);
  bool addEspNowPeer(const uint8_t mac[6]);
  bool isKnownRemote(const uint8_t mac[6]) const;
  bool isSelectedHome(const uint8_t mac[6]) const;
  bool canSendEvent(const uint8_t* payload, uint8_t payloadLength) const;
  bool sendEvent(const uint8_t destinationMac[6], uint8_t eventType,
                 const uint8_t* payload, uint8_t payloadLength);
  bool dequeuePacket(QueuedPacket& packet);
  void enqueuePacket(const uint8_t sourceMac[6], const uint8_t* data, int length);

  // ESP-IDF 5 changed the ESP-NOW receive callback's first parameter.
#if ESP_IDF_VERSION_MAJOR >= 5
  static void receiveCallback(const esp_now_recv_info_t* receiveInfo, const uint8_t* data, int length);
#else
  static void receiveCallback(const uint8_t* sourceMac, const uint8_t* data, int length);
#endif
  static NodeNetwork* instance_;
};
