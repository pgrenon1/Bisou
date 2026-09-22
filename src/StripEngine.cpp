#include "StripEngine.h"

// Configure the LEDs and buttons before a game starts running.
void StripEngine::begin()
{
  FastLED.addLeds<StripEngineConfig::LedChipset, StripEngineConfig::LedPin, StripEngineConfig::ColorOrder>(leds_, StripEngineConfig::LedCount);
  FastLED.setBrightness(StripEngineConfig::Brightness);

  pinMode(StripEngineConfig::LeftButtonPin, INPUT_PULLUP);
  pinMode(StripEngineConfig::RightButtonPin, INPUT_PULLUP);

  randomSeed(analogRead(A0));

  clear();
  show();
}

// Run one complete game frame, then send it to the LED strip.
void StripEngine::update()
{
  if (activeGame_ == nullptr)
    return;

  activeGame_->update();
  activeGame_->render();

  show();
}

// Unmount the old game before handing the engine to the new one.
void StripEngine::mountGame(Game &game)
{
  if (activeGame_ == &game)
    return;

  if (activeGame_ != nullptr)
    activeGame_->onUnmount();

  activeGame_ = &game;
  clear();
  activeGame_->onMount(*this);
}

// INPUT_PULLUP buttons read LOW while pressed.
bool StripEngine::buttonIsPressed(Button button) const
{
  const uint8_t pin = button == Button::Left ? StripEngineConfig::LeftButtonPin : StripEngineConfig::RightButtonPin;
  return digitalRead(pin) == LOW;
}

// Send the prepared frame to the physical LED strip.
void StripEngine::show() { FastLED.show(); }

// Reset every LED to black before drawing a new scene.
void StripEngine::clear() { fill_solid(leds_, StripEngineConfig::LedCount, CRGB::Black); }
