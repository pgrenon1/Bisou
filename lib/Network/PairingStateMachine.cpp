#include "PairingStateMachine.h"
#include "NodeNetwork.h"
#include <WebConsole.h>

// Define state objects before registering their stable addresses with SimpleFSM.
PairingStateMachine::PairingStateMachine()
  : checkingBootHoldState_("Checking boot hold", enterCheckingBootHoldState, updateCheckingBootHoldState),
    unpairedState_("Unpaired", enterUnpairedState),
    seekingHomeState_("Seeking home", enterSeekingHomeState, updateSeekingHomeState),
    homePairingState_("Home pairing", enterHomePairingState, updateHomePairingState),
    pairedHomeState_("Paired home", NULL),
    pairedRemoteState_("Paired remote", NULL),
    states_{&checkingBootHoldState_, &unpairedState_, &seekingHomeState_, &homePairingState_,
            &pairedHomeState_, &pairedRemoteState_},
    transitions_{
      Transition(&checkingBootHoldState_, &seekingHomeState_, StartPairingEvent),
      Transition(&checkingBootHoldState_, &unpairedState_, BootCheckCompleteEvent),
      Transition(&seekingHomeState_, &homePairingState_, HomeDiscoveryTimedOutEvent),
      Transition(&seekingHomeState_, &unpairedState_, PairingWindowTimedOutEvent),
      Transition(&homePairingState_, &pairedHomeState_, PairingWindowTimedOutEvent),
      Transition(&homePairingState_, &pairedHomeState_, PairingCompleteEvent),
      Transition(&seekingHomeState_, &pairedRemoteState_, PairingAcceptedEvent),
      Transition(&checkingBootHoldState_, &pairedHomeState_, RestorePairedHomeEvent),
      Transition(&checkingBootHoldState_, &pairedRemoteState_, RestorePairedRemoteEvent)}
{
}

bool PairingStateMachine::begin()
{
  if (stateMachine_.add(states_, 6) != FSMError::OK ||
      stateMachine_.add(transitions_, 9) != FSMError::OK)
    return false;

  stateMachine_.setInitialState(&checkingBootHoldState_);
  return true;
}

void PairingStateMachine::update()
{
  stateMachine_.run(0);
}

bool PairingStateMachine::isPairing() const
{
  return isSeekingHome() || isHomePairing();
}

bool PairingStateMachine::isPairedHome() const
{
  return stateMachine_.isInState(&pairedHomeState_);
}

bool PairingStateMachine::isPairedRemote() const
{
  return stateMachine_.isInState(&pairedRemoteState_);
}

bool PairingStateMachine::isUnpaired() const
{
  return stateMachine_.isInState(&unpairedState_);
}

bool PairingStateMachine::isSeekingHome() const
{
  return stateMachine_.isInState(&seekingHomeState_);
}

bool PairingStateMachine::isHomePairing() const
{
  return stateMachine_.isInState(&homePairingState_);
}

void PairingStateMachine::startPairing()
{
  stateMachine_.trigger(StartPairingEvent);
}

void PairingStateMachine::completeBootCheck()
{
  stateMachine_.trigger(BootCheckCompleteEvent);
}

void PairingStateMachine::becomeHome()
{
  stateMachine_.trigger(HomeDiscoveryTimedOutEvent);
}

void PairingStateMachine::finishPairingWindow()
{
  stateMachine_.trigger(PairingWindowTimedOutEvent);
}

void PairingStateMachine::completePairing()
{
  stateMachine_.trigger(PairingCompleteEvent);
}

void PairingStateMachine::acceptPairing()
{
  stateMachine_.trigger(PairingAcceptedEvent);
}

void PairingStateMachine::restorePairedHome()
{
  stateMachine_.trigger(RestorePairedHomeEvent);
}

void PairingStateMachine::restorePairedRemote()
{
  stateMachine_.trigger(RestorePairedRemoteEvent);
}

void PairingStateMachine::enterCheckingBootHoldState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  nodeNetwork->bootHoldActive_ = nodeNetwork->pairingButton_.isPressed();
  nodeNetwork->bootCheckStartedAt_ = millis();
  Console.println("ESP-NOW ready. Checking pairing button");
}

void PairingStateMachine::enterUnpairedState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  nodeNetwork->role_ = NodeNetwork::Role::Unconfigured;
}

void PairingStateMachine::updateCheckingBootHoldState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  // A release at any time during the boot check cancels the pairing gesture.
  if (!nodeNetwork->pairingButton_.isPressed())
    nodeNetwork->bootHoldActive_ = false;

  if (millis() - nodeNetwork->bootCheckStartedAt_ >= NodeNetworkConfig::PairingHoldDurationMs)
  {
    if (nodeNetwork->bootHoldActive_)
      nodeNetwork->startPairing();
    else
      nodeNetwork->completeBootCheck();
  }
}

void PairingStateMachine::enterSeekingHomeState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  nodeNetwork->pairingStartedAt_ = millis();
  Console.println("Searching for a home node");
}

void PairingStateMachine::updateSeekingHomeState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  if (millis() - nodeNetwork->pairingStartedAt_ >= NodeNetworkConfig::PairingWindowDurationMs)
    nodeNetwork->finishPairingWindow();
  else if (millis() - nodeNetwork->pairingStartedAt_ >= NodeNetworkConfig::HomeDiscoveryDurationMs)
    nodeNetwork->becomeHome();
}

void PairingStateMachine::enterHomePairingState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  nodeNetwork->peerState_ = {};
  nodeNetwork->role_ = NodeNetwork::Role::Home;
  nodeNetwork->lastHomeAdvertisementAt_ = 0;
  Console.println("This node is the home node");
}

void PairingStateMachine::updateHomePairingState()
{
  NodeNetwork* nodeNetwork = NodeNetwork::instance_;
  if (nodeNetwork == nullptr)
    return;

  if (millis() - nodeNetwork->pairingStartedAt_ >= NodeNetworkConfig::PairingWindowDurationMs)
    nodeNetwork->finishPairingWindow();
  else if (millis() - nodeNetwork->lastHomeAdvertisementAt_ >= NodeNetworkConfig::HomeAdvertisementIntervalMs)
    nodeNetwork->sendHomeAdvertisement();
}
