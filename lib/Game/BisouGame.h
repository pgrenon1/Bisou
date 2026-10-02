#pragma once

#include "Game.h"

class BisouGame : public Game {
 public:
  void update() override;
  void render() override;
  void resetGame() override;

 private:
     void onMounted() override;
     static constexpr uint8_t backgroundBrightness = 2, initialTargetZoneBreathingRate = 28;
     static constexpr uint8_t targetZoneMinBrightness = 60, leftEndpointGlowRate = 30, leftEndpointMinBrightness = 100;
     static constexpr uint8_t rightEndpointGlowRate = 32, rightEndpointMinBrightness = 100;
     static constexpr int initialTargetZoneSize = 8, minTargetZoneSize = 1, targetZoneEdgeMargin = 10;
     static constexpr int targetZoneBreathingRateStep = 16, maxMissStreak = 3, missMarkerDurationFrames = 7;
     static constexpr unsigned long pulseMoveIntervalMs = 15;
     int leftPulsePosition_ = -1, rightPulsePosition_ = -1;
     bool leftPulseActive_ = false, rightPulseActive_ = false;
     int targetZoneStart_ = 0, targetZoneEnd_ = 0, currentTargetZoneSize_ = initialTargetZoneSize;
     int currentTargetZoneBreathingRate_ = initialTargetZoneBreathingRate, missStreak_ = 0;
     unsigned long lastPulseMoveTime_ = 0;
     bool missMarkerActive_ = false;
     int missMarkerCenter_ = 0, missMarkerFrame_ = 0;
     void relocateTargetZone();
     void removePulses();
     void handleInput();
     void movePulses();
     void fadeBackground();
     void checkCollision();
     void handleMiss(int center);
     void handleHit();
     void drawScene();
     void updateMissMarker();
     void playHitAnimation();
     void playProgressResetAnimation();
};
