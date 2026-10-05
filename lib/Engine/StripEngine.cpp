#include "StripEngine.h"

// Configure the LEDs and buttons before a game starts running.
void StripEngine::begin()
{
  nodeNetwork_ = new NodeNetwork();
  nodeNetwork_->begin();

  FastLED.addLeds<StripEngineConfig::LedChipset, StripEngineConfig::LedPin, StripEngineConfig::ColorOrder>(leds_, StripEngineConfig::LedCount);
  FastLED.setBrightness(StripEngineConfig::Brightness);

  randomSeed(analogRead(StripEngineConfig::RandomSeedPin));
  clear();
  show();
}

// Run one complete game frame, then send it to the LED strip.
void StripEngine::update()
{
  nodeNetwork_->update();

  if (activeGame_ == nullptr)
    return;

  activeGame_->update();
  activeGame_->render();

  show();
}

// A remote node must never mount or execute a game.
void StripEngine::mountGame(Game &game)
{
  if (!isHomeReady() || activeGame_ == &game)
    return;

  if (activeGame_ != nullptr)
    activeGame_->onUnmount();

  activeGame_ = &game;
  clear();
  activeGame_->onMount(*this);
  Serial.println("Game mounted");
}

bool StripEngine::isHomeReady() const
{
  return nodeNetwork_ != nullptr && nodeNetwork_->isHomeReady();
}

const NodeNetwork::InputState& StripEngine::homeInputState() const
{
  return nodeNetwork_->homeInputState();
}

const NodeNetwork::InputState* StripEngine::remoteInputState(uint8_t remoteIndex) const
{
  return nodeNetwork_->remoteInputState(remoteIndex);
}

// Send the prepared frame to the physical LED strip.
void StripEngine::show() { FastLED.show(); }

// Reset every LED to black before drawing a new scene.
void StripEngine::clear() { fill_solid(leds_, StripEngineConfig::LedCount, CRGB::Black); }

bool StripEngine::hasActiveGame() { activeGame_ != nullptr; }
