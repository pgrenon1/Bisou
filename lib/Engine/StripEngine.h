#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include <Game.h>
#include "StripEngineConfig.h"

class StripEngine {
 public:
  enum class Button { Left, Right };

  void begin();
  void update();
  void mountGame(Game& game);
  CRGB* leds() { return leds_; }
  uint16_t ledCount() const { return StripEngineConfig::LedCount; }
  bool buttonIsPressed(Button button) const;
  void show();
  void clear();
  void log(const char* message) const;

  private:
  CRGB leds_[StripEngineConfig::LedCount];
  Game* activeGame_ = nullptr;
};
