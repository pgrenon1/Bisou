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
  engine.mountGame(bisouGame);
}

void loop()
{
  otaProgrammer.update();
  engine.update();
}
