#include <Arduino.h>
#include <OTA.h>
#include <BisouGame.h>
#include <StripEngine.h>

StripEngine engine;
BisouGame bisouGame;
OTA otaProgrammer;

void setup()
{
  Serial.begin(115200);
  otaProgrammer.begin();
  engine.begin();

  Serial.println(ESP.getChipModel());
}

void loop()
{
  otaProgrammer.update();
  engine.update();

  // Wait for pairing and role detection before allowing the home to run the game.
  if (engine.isHomeReady() && !engine.hasActiveGame())
  {
    engine.mountGame(bisouGame);
  }
}
