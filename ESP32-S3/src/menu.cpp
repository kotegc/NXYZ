#include "menu.h"

Menu::Menu(LGFX* gfx, AiEsp32RotaryEncoder* encoder)
  : _gfx(gfx), _encoder(encoder) {}

void Menu::draw(PhysicsModule** modules, int count, int selected) {
  _gfx->fillScreen(TFT_BLACK);
  _gfx->setTextSize(2);

  _gfx->setTextColor(TFT_WHITE);
  _gfx->setCursor(10, 10);
  _gfx->printf("SELECT MODULE");

  for (int i = 0; i < count; i++) {
    if (i == selected) {
      _gfx->setTextColor(TFT_BLACK);
      _gfx->fillRect(8, 36 + i * 28, 304, 24, TFT_WHITE);
    } else {
      _gfx->setTextColor(TFT_WHITE);
    }
    _gfx->setCursor(12, 40 + i * 28);
    _gfx->printf("%s", modules[i]->name());
  }
}

PhysicsModule* Menu::run(PhysicsModule** modules, int count) {
  int selected = 0;

  _encoder->setBoundaries(0, count - 1, false);
  _encoder->setEncoderValue(0);

  draw(modules, count, selected);

  while (true) {
    if (_encoder->encoderChanged()) {
      selected = _encoder->readEncoder();
      draw(modules, count, selected);
    }
    if (_encoder->isEncoderButtonClicked()) {
      delay(200);  // debounce
      return modules[selected];
    }
    delay(10);
  }
}