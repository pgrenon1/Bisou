#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include <Game.h>
#include "StripEngineConfig.h"
#include "NodeNetwork.h"

class StripEngine {
 public:
  void begin();
  void update();
  void mountGame(Game& game);
  bool isHomeReady() const;
  CRGB* leds() { return leds_; }
  uint16_t ledCount() const { return StripEngineConfig::LedCount; }
  const NodeNetwork::InputState& homeInputState() const;
  const NodeNetwork::InputState* remoteInputState(uint8_t remoteIndex) const;
  void show();
  void clear();
  bool hasActiveGame();

  private:
  CRGB leds_[StripEngineConfig::LedCount];
  Game* activeGame_ = nullptr;
  NodeNetwork* nodeNetwork_ = nullptr;
};
