#include "NodeNetwork.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <OTAConfig.h>
#include <WebConsole.h>

NodeNetwork* NodeNetwork::instance_ = nullptr;

namespace
{
  constexpr char PairingStorageNamespace[] = "node-network";
  constexpr char PairingStorageKey[] = "pairing";
}

void NodeNetwork::begin()
{
  setupInputButton();
  setupPairingButton();
  pinMode(NodeNetworkConfig::StatusLedPin, OUTPUT);
  digitalWrite(NodeNetworkConfig::StatusLedPin, LOW);

  // OTA starts Wi-Fi first; keep ESP-NOW on that same station interface and channel.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  esp_wifi_set_channel(OTAConfig::WifiChannel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Console.println("ESP-NOW initialization failed");
    return;
  }

  instance_ = this;
  esp_now_register_recv_cb(receiveCallback);
  setupPairingStateMachine();
  started_ = true;
}

void NodeNetwork::update()
{
  if (!started_)
    return;

  updateButtons();
  processReceivedPackets();
  updatePairingStateMachine();
  updateLocalInputState();
  updateStatusLed();
}

void NodeNetwork::setupInputButton()
{
  inputButton_.begin(NodeNetworkConfig::InputButtonPin);
  inputButton_.setDebounceTime(NodeNetworkConfig::InputDebounceTimeMs);
  inputButton_.setContext(this);
  inputButton_.setPressedHandler(inputPressedHandler);
  inputButton_.setReleasedHandler(inputReleasedHandler);
}

void NodeNetwork::setupPairingButton()
{
  pairingButton_.begin(NodeNetworkConfig::PairingButtonPin);
  pairingButton_.setDebounceTime(NodeNetworkConfig::PairingDebounceTimeMs);
}

void NodeNetwork::setupPairingStateMachine()
{
  if (!pairingStateMachine_.begin())
  {
    Console.println("Failed to configure pairing state machine");
    return;
  }
}

void NodeNetwork::updateButtons()
{
  inputButton_.loop();
  pairingButton_.loop();
}

void NodeNetwork::processReceivedPackets()
{
  QueuedPacket packet;
  while (dequeuePacket(packet))
    handlePacket(packet);
}

void NodeNetwork::updatePairingStateMachine()
{
  pairingStateMachine_.update();
}

bool NodeNetwork::isPairing() const
{
  return pairingStateMachine_.isPairing();
}

bool NodeNetwork::isHomeReady() const
{
  return role_ == Role::Home && pairingStateMachine_.isPairedHome();
}

void NodeNetwork::completePairing()
{
  if (role_ != Role::Home || peerState_.remoteCount == 0 || !pairingStateMachine_.isHomePairing())
    return;

  pairingStateMachine_.completePairing();
  savePairingStorage();
}

uint8_t NodeNetwork::remoteCount() const
{
  return role_ == Role::Home ? peerState_.remoteCount : 0;
}

const NodeNetwork::InputState* NodeNetwork::remoteInputState(uint8_t remoteIndex) const
{
  if (role_ != Role::Home || remoteIndex >= peerState_.remoteCount)
    return nullptr;

  return &peerState_.remoteNodes[remoteIndex].inputState;
}

bool NodeNetwork::sendToHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength)
{
  if (role_ != Role::Remote || !canSendEvent(payload, payloadLength))
  {
    Console.println("Unable to send event to home");
    return false;
  }

  return sendEvent(peerState_.homeMac, eventType, payload, payloadLength);
}

bool NodeNetwork::sendToRemote(const uint8_t remoteMac[6], uint8_t eventType,
                               const uint8_t* payload, uint8_t payloadLength)
{
  if (role_ != Role::Home || !isKnownRemote(remoteMac) || !canSendEvent(payload, payloadLength))
  {
    Console.println("Unable to send event to remote");
    return false;
  }

  return sendEvent(remoteMac, eventType, payload, payloadLength);
}

void NodeNetwork::broadcastFromHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength)
{
  if (role_ != Role::Home || !canSendEvent(payload, payloadLength))
    return;

  for (uint8_t i = 0; i < peerState_.remoteCount; ++i)
    sendEvent(peerState_.remoteNodes[i].mac, eventType, payload, payloadLength);
}

void NodeNetwork::startPairing()
{
  clearPairingStorage();
  pairingStateMachine_.startPairing();
}

void NodeNetwork::completeBootCheck()
{
  if (loadPairingStorage())
    return;

  Console.println("No saved pairing");
  pairingStateMachine_.completeBootCheck();
}

void NodeNetwork::becomeHome()
{
  pairingStateMachine_.becomeHome();
}

void NodeNetwork::finishRemotePairing(const uint8_t homeMac[6])
{
  peerState_ = {};
  memcpy(peerState_.homeMac, homeMac, sizeof(peerState_.homeMac));
  role_ = Role::Remote;
  addEspNowPeer(peerState_.homeMac);
  Console.println("Paired as a remote node");
  pairingStateMachine_.acceptPairing();
  savePairingStorage();
}

void NodeNetwork::finishPairingWindow()
{
  Console.println("Pairing window ended");
  pairingStateMachine_.finishPairingWindow();
}

bool NodeNetwork::loadPairingStorage()
{
  Preferences preferences;
  preferences.begin(PairingStorageNamespace, true);

  PairingStorage storage = {};
  const bool hasStorage = preferences.getBytesLength(PairingStorageKey) == sizeof(storage) &&
                          preferences.getBytes(PairingStorageKey, &storage, sizeof(storage)) == sizeof(storage);
  preferences.end();

  if (!hasStorage || storage.peerState.remoteCount > NodeNetworkConfig::MaxRemoteNodes)
    return false;

  const Role storedRole = static_cast<Role>(storage.role);
  if (storedRole != Role::Home && storedRole != Role::Remote)
    return false;

  peerState_ = storage.peerState;
  role_ = storedRole;
  if (!addStoredPeers())
  {
    peerState_ = {};
    role_ = Role::Unconfigured;
    return false;
  }

  if (role_ == Role::Home)
    pairingStateMachine_.restorePairedHome();
  else
    pairingStateMachine_.restorePairedRemote();

  Console.println("Saved pairing restored");
  return true;
}

bool NodeNetwork::savePairingStorage() const
{
  const bool homePairingComplete = role_ == Role::Home && peerState_.remoteCount > 0;
  const bool remotePairingComplete = role_ == Role::Remote;
  if (!homePairingComplete && !remotePairingComplete)
    return false;

  PairingStorage storage = {static_cast<uint8_t>(role_), peerState_};
  Preferences preferences;
  preferences.begin(PairingStorageNamespace, false);
  const size_t bytesWritten = preferences.putBytes(PairingStorageKey, &storage, sizeof(storage));
  preferences.end();

  if (bytesWritten != sizeof(storage))
  {
    Console.println("Failed to save pairing");
    return false;
  }

  Console.println("Pairing saved");
  return true;
}

void NodeNetwork::clearPairingStorage()
{
  Preferences preferences;
  preferences.begin(PairingStorageNamespace, false);
  preferences.remove(PairingStorageKey);
  preferences.end();
}

bool NodeNetwork::addStoredPeers()
{
  if (role_ == Role::Remote)
    return addEspNowPeer(peerState_.homeMac);

  if (role_ == Role::Home)
  {
    for (uint8_t i = 0; i < peerState_.remoteCount; ++i)
      if (!addEspNowPeer(peerState_.remoteNodes[i].mac))
        return false;
  }

  return true;
}

void NodeNetwork::sendHomeAdvertisement()
{
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  addEspNowPeer(broadcastMac);

  PacketHeader packet = {PacketMarker, PacketType::HomeAdvertisement};
  esp_now_send(broadcastMac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  lastHomeAdvertisementAt_ = millis();
}

// Read the physical input and send a remote node's initial state plus later transitions.
void NodeNetwork::updateLocalInputState()
{
  const bool buttonPressed = debouncedInputPressed_;
  if (role_ != Role::Remote)
  {
    homeInputState_.buttonPressed = buttonPressed;
    return;
  }

  if (remoteLocalInputState_.buttonPressed == buttonPressed && remoteInputStateSent_)
    return;

  remoteLocalInputState_.buttonPressed = buttonPressed;

  if (pairingStateMachine_.isPairedRemote())
  {
    const uint8_t payload[] = {static_cast<uint8_t>(buttonPressed)};
    remoteInputStateSent_ = sendToHome(InputStateEventType, payload, sizeof(payload));
  }
}

// Button2 invokes these only after its configured debounce interval has elapsed.
void NodeNetwork::inputPressedHandler(Button2& button)
{
  NodeNetwork* nodeNetwork = static_cast<NodeNetwork*>(button.getContext());
  if (nodeNetwork != nullptr)
    nodeNetwork->debouncedInputPressed_ = true;
}

void NodeNetwork::inputReleasedHandler(Button2& button)
{
  NodeNetwork* nodeNetwork = static_cast<NodeNetwork*>(button.getContext());
  if (nodeNetwork != nullptr)
    nodeNetwork->debouncedInputPressed_ = false;
}

// Update the state owned by the home node for one of its paired remote nodes.
void NodeNetwork::updateRemoteInputState(const uint8_t sourceMac[6], bool buttonPressed)
{
  for (uint8_t i = 0; i < peerState_.remoteCount; ++i)
  {
    if (sameMac(peerState_.remoteNodes[i].mac, sourceMac))
    {
      peerState_.remoteNodes[i].inputState.buttonPressed = buttonPressed;
      return;
    }
  }
}

// Show each network role without delaying packet handling or the game loop.
void NodeNetwork::updateStatusLed()
{
  const uint32_t now = millis();
  bool isOn = false;

  if (isPairing())
    isOn = (now / NodeNetworkConfig::StatusLedPairingFlashPeriodMs) % 2 == 0;
  else if (role_ == Role::Home)
    isOn = true;
  else if (role_ == Role::Remote)
    isOn = now % NodeNetworkConfig::StatusLedHeartbeatPeriodMs < NodeNetworkConfig::StatusLedHeartbeatOnDurationMs;
  else if (role_ == Role::Unconfigured)
    isOn = false;


  digitalWrite(NodeNetworkConfig::StatusLedPin, isOn ? HIGH : LOW);
}

void NodeNetwork::handlePacket(const QueuedPacket& packet)
{
  if (packet.length < sizeof(PacketHeader))
    return;

  const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packet.data);
  if (header->marker != PacketMarker)
    return;

  if (header->type == PacketType::HomeAdvertisement && pairingStateMachine_.isSeekingHome())
  {
    // The first discovered home is the only pairing target for this boot window.
    if (isSelectedHome(packet.sourceMac))
      return;

    memcpy(selectedHomeMac_, packet.sourceMac, sizeof(selectedHomeMac_));
    addEspNowPeer(selectedHomeMac_);

    PacketHeader response = {PacketMarker, PacketType::PairRequest};
    esp_now_send(selectedHomeMac_, reinterpret_cast<const uint8_t*>(&response), sizeof(response));
    Console.println("Home node found; pairing requested");
    return;
  }

  if (header->type == PacketType::PairRequest && pairingStateMachine_.isHomePairing())
  {
    if (!isKnownRemote(packet.sourceMac) && peerState_.remoteCount < NodeNetworkConfig::MaxRemoteNodes)
    {
      PeerState::RemoteNode& remote = peerState_.remoteNodes[peerState_.remoteCount];
      memcpy(remote.mac, packet.sourceMac, sizeof(remote.mac));
      remote.inputState = InputState(false, false);
      ++peerState_.remoteCount;
      addEspNowPeer(packet.sourceMac);
      Console.println("Remote node paired");
    }
    else if (!isKnownRemote(packet.sourceMac))
    {
      Console.println("Remote pairing rejected; node limit reached");
    }

    if (isKnownRemote(packet.sourceMac))
    {
      PacketHeader response = {PacketMarker, PacketType::PairAccepted};
      esp_now_send(packet.sourceMac, reinterpret_cast<const uint8_t*>(&response), sizeof(response));
    }
    return;
  }

  if (header->type == PacketType::PairAccepted && pairingStateMachine_.isSeekingHome() && isSelectedHome(packet.sourceMac))
  {
    finishRemotePairing(packet.sourceMac);
    return;
  }

  if (header->type == PacketType::Event)
    handleEvent(packet);
}

void NodeNetwork::handleEvent(const QueuedPacket& packet)
{
  if (packet.length < sizeof(PacketHeader) + 2)
    return;

  const EventPacket* event = reinterpret_cast<const EventPacket*>(packet.data);
  const uint16_t expectedLength = sizeof(PacketHeader) + 2 + event->payloadLength;
  if (event->payloadLength > NodeNetworkConfig::MaxEventPayloadBytes || packet.length != expectedLength)
    return;

  // Runtime events are accepted only from peers paired during this session.
  const bool accepted = (role_ == Role::Home && isKnownRemote(packet.sourceMac)) ||
                        (role_ == Role::Remote && isSelectedHome(packet.sourceMac));
  if (accepted)
  {
    // Include enough context to identify the peer and decode input events on serial.
    if (event->eventType == InputStateEventType && event->payloadLength == 1 && event->payload[0] <= 1)
    {
      Console.printf("ESP-NOW event: source=%02X:%02X:%02X:%02X:%02X:%02X type=input-state button=%s\n",
                    packet.sourceMac[0], packet.sourceMac[1], packet.sourceMac[2],
                    packet.sourceMac[3], packet.sourceMac[4], packet.sourceMac[5],
                    event->payload[0] == 0 ? "up" : "down");
    }
    else
    {
      Console.printf("ESP-NOW event: source=%02X:%02X:%02X:%02X:%02X:%02X type=%u payload=%u bytes\n",
                    packet.sourceMac[0], packet.sourceMac[1], packet.sourceMac[2],
                    packet.sourceMac[3], packet.sourceMac[4], packet.sourceMac[5],
                    event->eventType, event->payloadLength);
    }

    // Input-state events are consumed by the home before application events are dispatched.
    if (role_ == Role::Home && event->eventType == InputStateEventType)
    {
      if (event->payloadLength == 1 && event->payload[0] <= 1)
        updateRemoteInputState(packet.sourceMac, event->payload[0] != 0);
      return;
    }

    if (eventHandler_ != nullptr)
      eventHandler_(packet.sourceMac, event->eventType, event->payload, event->payloadLength);
  }
}

bool NodeNetwork::addEspNowPeer(const uint8_t mac[6])
{
  if (esp_now_is_peer_exist(mac))
    return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, sizeof(peer.peer_addr));
  peer.channel = OTAConfig::WifiChannel;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK)
  {
    Console.println("Failed to add ESP-NOW peer");
    return false;
  }

  return true;
}

bool NodeNetwork::isKnownRemote(const uint8_t mac[6]) const
{
  if (role_ != Role::Home)
    return false;

  for (uint8_t i = 0; i < peerState_.remoteCount; ++i)
    if (sameMac(peerState_.remoteNodes[i].mac, mac))
      return true;

  return false;
}

bool NodeNetwork::isSelectedHome(const uint8_t mac[6]) const
{
  return sameMac(selectedHomeMac_, mac) ||
         (role_ == Role::Remote && sameMac(peerState_.homeMac, mac));
}

bool NodeNetwork::sameMac(const uint8_t left[6], const uint8_t right[6]) const
{
  return memcmp(left, right, 6) == 0;
}

bool NodeNetwork::canSendEvent(const uint8_t* payload, uint8_t payloadLength) const
{
  return payloadLength <= NodeNetworkConfig::MaxEventPayloadBytes &&
         (payloadLength == 0 || payload != nullptr);
}

bool NodeNetwork::sendEvent(const uint8_t destinationMac[6], uint8_t eventType,
                            const uint8_t* payload, uint8_t payloadLength)
{
  EventPacket packet = {};
  packet.header = {PacketMarker, PacketType::Event};
  packet.eventType = eventType;
  packet.payloadLength = payloadLength;
  if (payloadLength > 0)
    memcpy(packet.payload, payload, payloadLength);

  const uint16_t length = sizeof(PacketHeader) + 2 + payloadLength;
  // Include the destination and decoded input state to mirror received-event logging.
  if (eventType == InputStateEventType && payloadLength == 1 && payload[0] <= 1)
  {
    Console.printf("ESP-NOW event sent: destination=%02X:%02X:%02X:%02X:%02X:%02X type=input-state button=%s\n",
                  destinationMac[0], destinationMac[1], destinationMac[2],
                  destinationMac[3], destinationMac[4], destinationMac[5],
                  payload[0] == 0 ? "up" : "down");
  }
  else
  {
    Console.printf("ESP-NOW event sent: destination=%02X:%02X:%02X:%02X:%02X:%02X type=%u payload=%u bytes\n",
                  destinationMac[0], destinationMac[1], destinationMac[2],
                  destinationMac[3], destinationMac[4], destinationMac[5],
                  eventType, payloadLength);
  }

  if (esp_now_send(destinationMac, reinterpret_cast<const uint8_t*>(&packet), length) != ESP_OK)
  {
    Console.println("ESP-NOW event send failed");
    return false;
  }

  return true;
}

bool NodeNetwork::dequeuePacket(QueuedPacket& packet)
{
  portENTER_CRITICAL(&receiveQueueMutex_);
  if (receiveTail_ == receiveHead_)
  {
    portEXIT_CRITICAL(&receiveQueueMutex_);
    return false;
  }

  packet = receiveQueue_[receiveTail_];
  receiveTail_ = (receiveTail_ + 1) % ReceiveQueueSize;
  portEXIT_CRITICAL(&receiveQueueMutex_);
  return true;
}

void NodeNetwork::enqueuePacket(const uint8_t sourceMac[6], const uint8_t* data, int length)
{
  if (length <= 0 || length > static_cast<int>(sizeof(receiveQueue_[0].data)))
    return;

  portENTER_CRITICAL(&receiveQueueMutex_);
  const uint8_t nextHead = (receiveHead_ + 1) % ReceiveQueueSize;
  if (nextHead != receiveTail_)
  {
    QueuedPacket& packet = receiveQueue_[receiveHead_];
    memcpy(packet.sourceMac, sourceMac, sizeof(packet.sourceMac));
    memcpy(packet.data, data, length);
    packet.length = length;
    receiveHead_ = nextHead;
  }
  portEXIT_CRITICAL(&receiveQueueMutex_);
}

// Keep ESP-NOW callback support compatible with both Arduino-ESP32 callback APIs.
#if ESP_IDF_VERSION_MAJOR >= 5
void NodeNetwork::receiveCallback(const esp_now_recv_info_t* receiveInfo, const uint8_t* data, int length)
{
  if (instance_ != nullptr && receiveInfo != nullptr)
    instance_->enqueuePacket(receiveInfo->src_addr, data, length);
}
#else
void NodeNetwork::receiveCallback(const uint8_t* sourceMac, const uint8_t* data, int length)
{
  if (instance_ != nullptr)
    instance_->enqueuePacket(sourceMac, data, length);
}
#endif
