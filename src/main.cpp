#include <Arduino.h>
#include <OTA.h>
#include <WebConsole.h>
#include <BisouGame.h>
#include <StripEngine.h>

StripEngine engine;
BisouGame bisouGame;
OTA otaProgrammer;

void setup()
{
  Serial.begin(115200);

  Console.println("===============================================");
  Console.println("Boot sequence started");

  Console.print("ChipModel: ");
  Console.println(ESP.getChipModel());

  otaProgrammer.begin();
  engine.begin();
}

void loop()
{
  otaProgrammer.update();
  engine.update();

  // Mount as soon as the node becomes home; mounting ends its pairing window.
  if (engine.isHome() && !engine.hasActiveGame())
  {
    engine.mountGame(bisouGame);
  }
}
