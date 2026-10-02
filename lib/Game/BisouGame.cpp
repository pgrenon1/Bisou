#include <Arduino.h>
#include "BisouGame.h"
#include <StripEngine.h>

// Reset the game whenever it becomes the active game.
void BisouGame::onMounted()
{
  resetGame();
  engine().log("Bisou game ready");
}

void BisouGame::resetGame()
{
  currentTargetZoneSize_ = initialTargetZoneSize;
  currentTargetZoneBreathingRate_ = initialTargetZoneBreathingRate;
  missStreak_ = 0;

  removePulses();
  relocateTargetZone();
}

// Advance input, pulse positions, and the fading background each frame.
void BisouGame::update()
{
  handleInput();

  movePulses();
  fadeBackground();
}

// Draw the current game state after the frame has been updated.
void BisouGame::render()
{
  drawScene();
  updateMissMarker();
}

// Choose a new target location while keeping it away from strip endpoints.
void BisouGame::relocateTargetZone()
{
  targetZoneStart_ = random(targetZoneEdgeMargin, engine().ledCount() - currentTargetZoneSize_ - targetZoneEdgeMargin + 1);
  targetZoneEnd_ = targetZoneStart_ + currentTargetZoneSize_ - 1;
}

// Deactivate both travelling pulses and clear their positions.
void BisouGame::removePulses()
{
  leftPulseActive_ = rightPulseActive_ = false;
  leftPulsePosition_ = rightPulsePosition_ = -1;
}

// Launch a pulse from an endpoint when its matching button is pressed.
void BisouGame::handleInput()
{
  if (!leftPulseActive_ && engine().buttonIsPressed(StripEngine::Button::Left))
  {
    leftPulseActive_ = true;
    leftPulsePosition_ = 0;
    engine().log("Left pulse launched");
  }
  if (!rightPulseActive_ && engine().buttonIsPressed(StripEngine::Button::Right))
  {
    rightPulseActive_ = true;
    rightPulsePosition_ = engine().ledCount() - 1;
    engine().log("Right pulse launched");
  }
}

// Move active pulses on a fixed interval and resolve crossings or exits.
void BisouGame::movePulses()
{
  // Throttle movement so pulse speed does not depend on frame rate.
  if (millis() - lastPulseMoveTime_ < pulseMoveIntervalMs)
    return;

  lastPulseMoveTime_ = millis();

  if (leftPulseActive_)
    ++leftPulsePosition_;
  if (rightPulseActive_)
    --rightPulsePosition_;

  // A crossing is evaluated before pulses that have left the strip are removed.
  if (leftPulseActive_ && rightPulseActive_ && leftPulsePosition_ >= rightPulsePosition_)
    checkCollision();

  if (leftPulsePosition_ >= static_cast<int>(engine().ledCount()))
    leftPulseActive_ = false;
  if (rightPulsePosition_ < 0)
    rightPulseActive_ = false;
}

// Dim the previous frame without allowing the strip to become fully dark.
void BisouGame::fadeBackground()
{
  CRGB *leds = engine().leds();

  for (uint16_t i = 0; i < engine().ledCount(); ++i)
  {
    leds[i].fadeToBlackBy(10);

    // Clamp each channel to retain the neutral background glow.
    if (leds[i].r < backgroundBrightness)
      leds[i].r = backgroundBrightness;
    if (leds[i].g < backgroundBrightness)
      leds[i].g = backgroundBrightness;
    if (leds[i].b < backgroundBrightness)
      leds[i].b = backgroundBrightness;
  }
}

// Classify the overlap point as a hit inside the target zone or a miss outside it.
void BisouGame::checkCollision()
{
  const int start = min(leftPulsePosition_, rightPulsePosition_), end = max(leftPulsePosition_, rightPulsePosition_);

  if (end >= targetZoneStart_ && start <= targetZoneEnd_)
    handleHit();
  else
    handleMiss((start + end) / 2);
}

// Record a miss, or reset progress after too many consecutive misses.
void BisouGame::handleMiss(int center)
{
  if (++missStreak_ < maxMissStreak)
  {
    engine().log("Miss");
    missMarkerActive_ = true;
    missMarkerCenter_ = center;
    missMarkerFrame_ = 0;
  }
  else
  {
    engine().log("Miss limit reached; progress reset");
    playProgressResetAnimation();

    currentTargetZoneSize_ = initialTargetZoneSize;
    currentTargetZoneBreathingRate_ = initialTargetZoneBreathingRate;
    missStreak_ = 0;

    relocateTargetZone();
  }

  removePulses();
}

// Reward a hit by increasing the target's difficulty and moving it.
void BisouGame::handleHit()
{
  engine().log("Hit");
  missStreak_ = 0;
  playHitAnimation();

  // Stop increasing difficulty once the target reaches its minimum size.
  if (currentTargetZoneSize_ > minTargetZoneSize)
  {
    --currentTargetZoneSize_;
    currentTargetZoneBreathingRate_ += targetZoneBreathingRateStep;
  }

  relocateTargetZone();
  removePulses();
}

// Render target, idle endpoints, and active pulses onto the LED buffer.
void BisouGame::drawScene()
{
  CRGB *leds = engine().leds();
  const uint8_t zone = beatsin8(currentTargetZoneBreathingRate_, targetZoneMinBrightness, 255);

  // The target's brightness oscillates while its hue remains yellow.
  for (int i = targetZoneStart_; i <= targetZoneEnd_; ++i)
    leds[i] = CHSV(40, 255, zone);

  // An idle endpoint pulses to show where the next launch can originate.
  if (!leftPulseActive_)
    leds[0] = CRGB(beatsin8(leftEndpointGlowRate, leftEndpointMinBrightness, 255));
  if (!rightPulseActive_)
    leds[engine().ledCount() - 1] = CRGB(beatsin8(rightEndpointGlowRate, rightEndpointMinBrightness, 255));

  // Bounds checks prevent drawing a pulse that moved off the strip this frame.
  if (leftPulseActive_ && leftPulsePosition_ >= 0 && leftPulsePosition_ < static_cast<int>(engine().ledCount()))
    leds[leftPulsePosition_] = CRGB::White;
  if (rightPulseActive_ && rightPulsePosition_ >= 0 && rightPulsePosition_ < static_cast<int>(engine().ledCount()))
    leds[rightPulsePosition_] = CRGB::White;
}

// Briefly show a red three-pixel marker at the latest missed collision point.
void BisouGame::updateMissMarker()
{
  if (!missMarkerActive_)
    return;

  CRGB *leds = engine().leds();

  for (int o = -1; o <= 1; ++o)
  {
    const int p = missMarkerCenter_ + o;

    // Clip the marker when the collision was near either strip edge.
    if (p >= 0 && p < static_cast<int>(engine().ledCount()))
      leds[p] = CRGB::Red;
  }

  if (++missMarkerFrame_ >= missMarkerDurationFrames)
    missMarkerActive_ = false;
}

// Play the green confirmation animation after a successful collision.
void BisouGame::playHitAnimation()
{
  CRGB *leds = engine().leds();

  // Fade in to bright green, settle at a dimmer green, then fade out.
  for (int i = 0; i <= 10; ++i)
  {
    fill_solid(leds, engine().ledCount(), CRGB(0, 255L * i / 10, 0));
    engine().show();
    delay(20);
  }

  for (int i = 0; i <= 10; ++i)
  {
    fill_solid(leds, engine().ledCount(), CRGB(0, 255 - (191L * i / 10), 0));
    engine().show();
    delay(20);
  }

  for (int i = 0; i <= 15; ++i)
  {
    fill_solid(leds, engine().ledCount(), CRGB(0, 64 + (191L * i / 15), 0));
    engine().show();
    delay(20);
  }

  for (int i = 0; i <= 30; ++i)
  {
    fill_solid(leds, engine().ledCount(), CRGB(0, 255 - (255L * i / 30), 0));
    engine().show();
    delay(20);
  }
}

// Blink red several times to signal that accumulated progress was reset.
void BisouGame::playProgressResetAnimation()
{
  for (int i = 0; i < 16; ++i)
  {
    // Alternate two bright frames with two dim frames.
    const uint8_t b = i % 4 < 2 ? 255 : 50;
    fill_solid(engine().leds(), engine().ledCount(), CRGB(b, 0, 0));
    engine().show();
    delay(40);
  }
}

