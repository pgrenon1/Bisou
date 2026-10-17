#pragma once

#include <SimpleFSM.h>

// Owns the SimpleFSM objects that control one node's pairing lifecycle.
class PairingStateMachine
{
 public:
  PairingStateMachine();
  bool begin();
  void update();
  bool isPairing() const;
  bool isPairedHome() const;
  bool isPairedRemote() const;
  bool isUnpaired() const;
  bool isSeekingHome() const;
  bool isHomePairing() const;
  void startPairing();
  void completeBootCheck();
  void becomeHome();
  void finishPairingWindow();
  void completePairing();
  void acceptPairing();
  void restorePairedHome();
  void restorePairedRemote();

 private:
  enum Event : uint8_t
  {
    StartPairingEvent = 1,
    BootCheckCompleteEvent,
    HomeDiscoveryTimedOutEvent,
    PairingWindowTimedOutEvent,
    PairingCompleteEvent,
    PairingAcceptedEvent,
    RestorePairedHomeEvent,
    RestorePairedRemoteEvent
  };

  mutable State checkingBootHoldState_;
  mutable State unpairedState_;
  mutable State seekingHomeState_;
  mutable State homePairingState_;
  mutable State pairedHomeState_;
  mutable State pairedRemoteState_;
  State* states_[6];
  Transition transitions_[9];
  SimpleFSM stateMachine_;

  static void enterCheckingBootHoldState();
  static void updateCheckingBootHoldState();
  static void enterSeekingHomeState();
  static void updateSeekingHomeState();
  static void enterHomePairingState();
  static void updateHomePairingState();
  static void enterUnpairedState();
};
