#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <AiEsp32RotaryEncoder.h>

#include "module_base.h"
#include "menu.h"
#include "modules/particles.h"
#include "modules/pendulum.h"
#include "lgfx_config.h"

// ── Hardware ──────────────────────────────────────────────
LGFX                  gfx;
AiEsp32RotaryEncoder  rotaryEncoder(5, 4, 6, -1, 4); // DT, CLK, SW

#define MENU_BTN_PIN  7

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// ── Modules ───────────────────────────────────────────────
ParticleModule        particleModule;
PendulumModule        pendulumModule;

PhysicsModule* modules[] = {
  &particleModule,
  // &polygonModule,   ← add later
  &pendulumModule,
  // &greyscottModule,
  // &chladniModule,
};
const int MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

// ── State ─────────────────────────────────────────────────
PhysicsModule* activeModule = nullptr;
Menu*          menu         = nullptr;

bool menuButtonPressed() {
  static uint32_t lastPress = 0;
  if (digitalRead(MENU_BTN_PIN) == LOW) {
    if (millis() - lastPress > 300) {
      lastPress = millis();
      return true;
    }
  }
  return false;
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("READY");

  pinMode(MENU_BTN_PIN, INPUT_PULLUP);

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setAcceleration(0);

  gfx.init();
  gfx.setRotation(1);
  gfx.fillScreen(TFT_BLACK);

  menu = new Menu(&gfx, &rotaryEncoder);

  // Launch directly into menu on boot
  activeModule = menu->run(modules, MODULE_COUNT);
  activeModule->init(&gfx, &rotaryEncoder);
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
  if (menuButtonPressed()) {
    if (activeModule) {
      activeModule->stop();
      activeModule = nullptr;
    }
    gfx.fillScreen(TFT_BLACK);
    activeModule = menu->run(modules, MODULE_COUNT);
    activeModule->init(&gfx, &rotaryEncoder);
  }

  if (activeModule) {
    activeModule->loop();
  }
}