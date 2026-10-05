#include "NodeNetwork.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <OTAConfig.h>

NodeNetwork* NodeNetwork::instance_ = nullptr;

namespace
{
  constexpr char StorageNamespace[] = "node-network";
  constexpr char StorageKey[] = "pairing";

  // Compare MAC addresses without converting them to temporary strings.
  bool sameMac(const uint8_t left[6], const uint8_t right[6])
  {
    return memcmp(left, right, 6) == 0;
  }
}

void NodeNetwork::begin()
{
  inputButton_.begin(NodeNetworkConfig::InputButtonPin);
  inputButton_.setDebounceTime(NodeNetworkConfig::InputDebounceTimeMs);
  inputButton_.setContext(this);
  inputButton_.setPressedHandler(inputPressedHandler);
  inputButton_.setReleasedHandler(inputReleasedHandler);
  pinMode(NodeNetworkConfig::StatusLedPin, OUTPUT);
  digitalWrite(NodeNetworkConfig::StatusLedPin, LOW);
  bootHoldActive_ = inputButton_.isPressed();
  bootCheckStartedAt_ = millis();

  // OTA starts Wi-Fi first; keep ESP-NOW on that same station interface and channel.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  esp_wifi_set_channel(OTAConfig::WifiChannel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  instance_ = this;
  esp_now_register_recv_cb(receiveCallback);
  loadStorage();
  started_ = true;
  Serial.println("ESP-NOW ready; checking pairing button");
}

void NodeNetwork::update()
{
  if (!started_)
    return;

  inputButton_.loop();

  // A release at any time during the boot check cancels the pairing gesture.
  if (state_ == State::CheckingBootHold)
  {
    if (!inputButton_.isPressed())
      bootHoldActive_ = false;

    if (millis() - bootCheckStartedAt_ >= NodeNetworkConfig::PairingHoldDurationMs)
    {
      if (bootHoldActive_)
      {
        clearPairingStorage();
        startPairing();
      }
      else
      {
        addStoredPeers();
        state_ = State::Normal;
        Serial.println("Stored pairing restored");
      }
    }
  }

  QueuedPacket packet;
  while (dequeuePacket(packet))
    handlePacket(packet);

  updateLocalInputState();

  if (isPairing() && millis() - pairingStartedAt_ >= NodeNetworkConfig::PairingWindowDurationMs)
  {
    state_ = State::Normal;
    Serial.println("Pairing window ended");
  }

  if (state_ == State::SeekingHome && millis() - pairingStartedAt_ >= NodeNetworkConfig::HomeDiscoveryDurationMs)
    becomeHome();

  if (state_ == State::HomePairing && millis() - lastHomeAdvertisementAt_ >= NodeNetworkConfig::HomeAdvertisementIntervalMs)
    sendHomeAdvertisement();

  updateStatusLed();
}

bool NodeNetwork::isPairing() const
{
  return state_ == State::SeekingHome || state_ == State::HomePairing;
}

bool NodeNetwork::isHomeReady() const
{
  return storedRole_ == Role::Home && state_ == State::Normal;
}

uint8_t NodeNetwork::remoteCount() const
{
  return storedRole_ == Role::Home ? storage_.remoteCount : 0;
}

const NodeNetwork::InputState* NodeNetwork::remoteInputState(uint8_t remoteIndex) const
{
  if (storedRole_ != Role::Home || remoteIndex >= storage_.remoteCount)
    return nullptr;

  return &storage_.remoteNodes[remoteIndex].inputState;
}

bool NodeNetwork::sendToHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength)
{
  if (storedRole_ != Role::Remote || !canSendEvent(payload, payloadLength))
  {
    Serial.println("Unable to send event to home");
    return false;
  }

  return sendEvent(storage_.homeMac, eventType, payload, payloadLength);
}

bool NodeNetwork::sendToRemote(const uint8_t remoteMac[6], uint8_t eventType,
                               const uint8_t* payload, uint8_t payloadLength)
{
  if (storedRole_ != Role::Home || !isKnownRemote(remoteMac) || !canSendEvent(payload, payloadLength))
  {
    Serial.println("Unable to send event to remote");
    return false;
  }

  return sendEvent(remoteMac, eventType, payload, payloadLength);
}

void NodeNetwork::broadcastFromHome(uint8_t eventType, const uint8_t* payload, uint8_t payloadLength)
{
  if (storedRole_ != Role::Home || !canSendEvent(payload, payloadLength))
    return;

  for (uint8_t i = 0; i < storage_.remoteCount; ++i)
    sendEvent(storage_.remoteNodes[i].mac, eventType, payload, payloadLength);
}

void NodeNetwork::loadStorage()
{
  Preferences preferences;
  preferences.begin(StorageNamespace, true);

  if (preferences.getBytesLength(StorageKey) == sizeof(storage_))
    preferences.getBytes(StorageKey, &storage_, sizeof(storage_));

  preferences.end();

  if (storage_.role == static_cast<uint8_t>(Role::Home) && storage_.remoteCount <= NodeNetworkConfig::MaxRemoteNodes)
  {
    storedRole_ = Role::Home;
    Serial.println("Loaded home pairing");
  }
  else if (storage_.role == static_cast<uint8_t>(Role::Remote))
  {
    storedRole_ = Role::Remote;
    Serial.println("Loaded remote pairing");
  }
  else
  {
    storage_ = {};
    Serial.println("No saved pairing");
  }
}

void NodeNetwork::saveStorage() const
{
  Preferences preferences;
  preferences.begin(StorageNamespace, false);
  preferences.putBytes(StorageKey, &storage_, sizeof(storage_));
  preferences.end();
}

void NodeNetwork::clearPairingStorage()
{
  // Remove only pairing data; game and other application settings are preserved.
  Preferences preferences;
  preferences.begin(StorageNamespace, false);
  preferences.clear();
  preferences.end();

  storage_ = {};
  storedRole_ = Role::Unconfigured;
  memset(selectedHomeMac_, 0, sizeof(selectedHomeMac_));
  Serial.println("Pairing data cleared");
}

void NodeNetwork::addStoredPeers()
{
  if (storedRole_ == Role::Remote)
    addEspNowPeer(storage_.homeMac);
  else if (storedRole_ == Role::Home)
    for (uint8_t i = 0; i < storage_.remoteCount; ++i)
      addEspNowPeer(storage_.remoteNodes[i].mac);
}

void NodeNetwork::startPairing()
{
  pairingStartedAt_ = millis();
  state_ = State::SeekingHome;
  Serial.println("Searching for a home node");
}

void NodeNetwork::becomeHome()
{
  storage_ = {};
  storage_.role = static_cast<uint8_t>(Role::Home);
  storedRole_ = Role::Home;
  saveStorage();
  state_ = State::HomePairing;
  lastHomeAdvertisementAt_ = 0;
  Serial.println("This node is the home node");
}

void NodeNetwork::finishRemotePairing(const uint8_t homeMac[6])
{
  storage_ = {};
  storage_.role = static_cast<uint8_t>(Role::Remote);
  memcpy(storage_.homeMac, homeMac, sizeof(storage_.homeMac));
  storedRole_ = Role::Remote;
  saveStorage();
  addEspNowPeer(storage_.homeMac);
  state_ = State::Normal;
  Serial.println("Paired as a remote node");
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
  if (storedRole_ != Role::Remote)
  {
    homeInputState_.buttonPressed = buttonPressed;
    return;
  }

  if (remoteLocalInputState_.buttonPressed == buttonPressed && remoteInputStateSent_)
    return;

  remoteLocalInputState_.buttonPressed = buttonPressed;

  if (state_ == State::Normal)
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
  for (uint8_t i = 0; i < storage_.remoteCount; ++i)
  {
    if (sameMac(storage_.remoteNodes[i].mac, sourceMac))
    {
      storage_.remoteNodes[i].inputState.buttonPressed = buttonPressed;
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
  else if (storedRole_ == Role::Home)
    isOn = true;
  else if (storedRole_ == Role::Remote)
    isOn = now % NodeNetworkConfig::StatusLedHeartbeatPeriodMs < NodeNetworkConfig::StatusLedHeartbeatOnDurationMs;
  else if (storedRole_ == Role::Unconfigured)
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

  if (header->type == PacketType::HomeAdvertisement && state_ == State::SeekingHome)
  {
    // The first discovered home is the only pairing target for this boot window.
    if (isSelectedHome(packet.sourceMac))
      return;

    memcpy(selectedHomeMac_, packet.sourceMac, sizeof(selectedHomeMac_));
    addEspNowPeer(selectedHomeMac_);

    PacketHeader response = {PacketMarker, PacketType::PairRequest};
    esp_now_send(selectedHomeMac_, reinterpret_cast<const uint8_t*>(&response), sizeof(response));
    Serial.println("Home node found; pairing requested");
    return;
  }

  if (header->type == PacketType::PairRequest && state_ == State::HomePairing)
  {
    if (!isKnownRemote(packet.sourceMac) && storage_.remoteCount < NodeNetworkConfig::MaxRemoteNodes)
    {
      PairingStorage::RemoteNode& remote = storage_.remoteNodes[storage_.remoteCount];
      memcpy(remote.mac, packet.sourceMac, sizeof(remote.mac));
      remote.inputState = InputState(false, false);
      ++storage_.remoteCount;
      saveStorage();
      addEspNowPeer(packet.sourceMac);
      Serial.println("Remote node paired");
    }
    else if (!isKnownRemote(packet.sourceMac))
    {
      Serial.println("Remote pairing rejected; node limit reached");
    }

    if (isKnownRemote(packet.sourceMac))
    {
      PacketHeader response = {PacketMarker, PacketType::PairAccepted};
      esp_now_send(packet.sourceMac, reinterpret_cast<const uint8_t*>(&response), sizeof(response));
    }
    return;
  }

  if (header->type == PacketType::PairAccepted && state_ == State::SeekingHome && isSelectedHome(packet.sourceMac))
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

  // Runtime events are only accepted from the peer relationship stored locally.
  const bool accepted = (storedRole_ == Role::Home && isKnownRemote(packet.sourceMac)) ||
                        (storedRole_ == Role::Remote && isSelectedHome(packet.sourceMac));
  if (accepted)
  {
    // Include enough context to identify the peer and decode input events on serial.
    if (event->eventType == InputStateEventType && event->payloadLength == 1 && event->payload[0] <= 1)
    {
      Serial.printf("ESP-NOW event: source=%02X:%02X:%02X:%02X:%02X:%02X type=input-state button=%s\n",
                    packet.sourceMac[0], packet.sourceMac[1], packet.sourceMac[2],
                    packet.sourceMac[3], packet.sourceMac[4], packet.sourceMac[5],
                    event->payload[0] == 0 ? "up" : "down");
    }
    else
    {
      Serial.printf("ESP-NOW event: source=%02X:%02X:%02X:%02X:%02X:%02X type=%u payload=%u bytes\n",
                    packet.sourceMac[0], packet.sourceMac[1], packet.sourceMac[2],
                    packet.sourceMac[3], packet.sourceMac[4], packet.sourceMac[5],
                    event->eventType, event->payloadLength);
    }

    // Input-state events are consumed by the home before application events are dispatched.
    if (storedRole_ == Role::Home && event->eventType == InputStateEventType)
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
    Serial.println("Failed to add ESP-NOW peer");
    return false;
  }

  return true;
}

bool NodeNetwork::isKnownRemote(const uint8_t mac[6]) const
{
  if (storedRole_ != Role::Home)
    return false;

  for (uint8_t i = 0; i < storage_.remoteCount; ++i)
    if (sameMac(storage_.remoteNodes[i].mac, mac))
      return true;

  return false;
}

bool NodeNetwork::isSelectedHome(const uint8_t mac[6]) const
{
  return sameMac(selectedHomeMac_, mac) ||
         (storedRole_ == Role::Remote && sameMac(storage_.homeMac, mac));
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
    Serial.printf("ESP-NOW event sent: destination=%02X:%02X:%02X:%02X:%02X:%02X type=input-state button=%s\n",
                  destinationMac[0], destinationMac[1], destinationMac[2],
                  destinationMac[3], destinationMac[4], destinationMac[5],
                  payload[0] == 0 ? "up" : "down");
  }
  else
  {
    Serial.printf("ESP-NOW event sent: destination=%02X:%02X:%02X:%02X:%02X:%02X type=%u payload=%u bytes\n",
                  destinationMac[0], destinationMac[1], destinationMac[2],
                  destinationMac[3], destinationMac[4], destinationMac[5],
                  eventType, payloadLength);
  }

  if (esp_now_send(destinationMac, reinterpret_cast<const uint8_t*>(&packet), length) != ESP_OK)
  {
    Serial.println("ESP-NOW event send failed");
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
