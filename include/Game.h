#pragma once

class StripEngine;

// Implement this interface for every game that can run on StripEngine.
class Game {
 public:
  virtual ~Game() = default;

  // Called by StripEngine when this game becomes active.
  void onMount(StripEngine& engine) { engine_ = &engine; onMounted(); }
  // Called before the game is replaced or detached from the engine.
  void onUnmount() { onUnmounted(); engine_ = nullptr; }
  // Advance the game state for one frame.
  virtual void update() = 0;
  // Draw the current state into the engine's LED buffer.
  virtual void render() = 0;
  // Reset the game state
  virtual void resetGame() = 0;

 protected:
  StripEngine& engine() { return *engine_; }

  virtual void onMounted() {}
  virtual void onUnmounted() {}

 protected:
  // Non-owning: valid only between onMount() and onUnmount().
  StripEngine* engine_ = nullptr;
};
