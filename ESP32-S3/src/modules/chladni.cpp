#include "chladni.h"
#include "uart_protocol.h"
#include "../event_bus.h"
#include "../ticks.h"
#include <math.h>

// ── Event manifest ─────────────────────────────────────────
static constexpr nxyz_protocol::EventDescriptor kChladniEvents[] = {
  { chladni_events::MODE_AMPLITUDE, nxyz_protocol::EventKind::Output,  "mode_amplitude" },
  { chladni_events::NODE_CROSSING,  nxyz_protocol::EventKind::Trigger, "node_crossing" },
};

const nxyz_protocol::EventDescriptor* ChladniModule::events() const { return kChladniEvents; }

// Standalone evaluator of drawPattern()'s formula at the plate center —
// kept separate from drawPattern()'s per-pixel loop so event emission
// never touches (and can't risk regressing) render performance.
float ChladniModule::sampleCenterValue(float t) {
  float m = 1.0f + t * 6.0f, n = 2.0f + t * 6.0f;
  float p = m + 1.0f,        q = n - 1.0f;
  float C = cosf(t * M_PI * 2.0f) * 0.5f + 0.5f;
  float D = sinf(t * M_PI * 2.0f) * 0.5f + 0.5f;
  float x = 0.5f, y = 0.5f; // plate center, same normalization as drawPattern
  return C * cosf(m * M_PI * x) * cosf(n * M_PI * y)
       + D * cosf(p * M_PI * x) * cosf(q * M_PI * y);
}

void ChladniModule::init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) {
  _gfx     = gfx;
  _encoder = encoder;

  _position         = 0;
  _lastEncoderValue = 0;
  _needsRedraw      = true;
  _fpsDirty         = false;
  _lastFPSVal       = 0;
  _prevCenterPositive = sampleCenterValue(0.f) >= 0.0f;

  _encoder->setBoundaries(0, MAX_POSITION, false);
  _encoder->setEncoderValue(0);
  _encoder->setAcceleration(10);

  _sprite = new LGFX_Sprite(_gfx);
  _sprite->setColorDepth(16);
  _sprite->createSprite(SCREEN_W, SCREEN_H);

  _gfx->fillScreen(TFT_WHITE);
  drawPattern();
  drawFPS(0);
}

void ChladniModule::stop() {
  if (_sprite) {
    _sprite->deleteSprite();
    delete _sprite;
    _sprite = nullptr;
  }
}

void ChladniModule::drawPattern() {
  float t = (float)_position / (float)MAX_POSITION;  // 0.0 → 1.0

  // Two superimposed modes that evolve as t increases
  // m,n sweep the base mode; p,q are the adjacent mode
  float m = 1.0f + t * 6.0f;
  float n = 2.0f + t * 6.0f;
  float p = m + 1.0f;
  float q = n - 1.0f;

  // Blend between the two modes sinusoidally
  // At t=0: pure first mode. As t increases both modes mix.
  float C = cosf(t * M_PI * 2.0f) * 0.5f + 0.5f;  // 1→0→1
  float D = sinf(t * M_PI * 2.0f) * 0.5f + 0.5f;  // 0→1→0

  float a = (float)PLATE_SIZE;
  float b = (float)PLATE_SIZE;

  _sprite->fillSprite(TFT_WHITE);

  for (int py = 0; py < SCREEN_H; py++) {
    float y = (float)(py + OFFSET_Y) / b;

    for (int px = 0; px < SCREEN_W; px++) {
      float x = (float)px / a;

      float val = C * cosf(m * M_PI * x) * cosf(n * M_PI * y)
                + D * cosf(p * M_PI * x) * cosf(q * M_PI * y);

      if (fabsf(val) < THRESHOLD) {
        _sprite->drawPixel(px, py, TFT_BLACK);
      }
    }
  }

  // Mode label
  _sprite->setTextColor(TFT_BLACK);
  _sprite->setTextSize(2);
  _sprite->setCursor(2, 2);
  _sprite->printf("m:%.1f n:%.1f", m, n);

  // FPS region clear so drawFPS can write over it
  _sprite->fillRect(FPS_X1, FPS_Y1,
                    FPS_X2 - FPS_X1, FPS_Y2 - FPS_Y1, TFT_WHITE);

  _sprite->pushSprite(0, 0);

  _needsRedraw = false;
  _fpsDirty    = true;
}

void ChladniModule::drawFPS(uint32_t fps) {
  _gfx->startWrite();
  _gfx->fillRect(FPS_X1, FPS_Y1,
                 FPS_X2 - FPS_X1, FPS_Y2 - FPS_Y1, TFT_WHITE);
  _gfx->setTextColor(TFT_BLACK);
  _gfx->setTextSize(2);
  _gfx->setCursor(FPS_X1 + 2, 2);
  _gfx->printf("FPS:%3u", fps);
  _gfx->endWrite();
}

void ChladniModule::loop() {

  // ── Encoder ──
  int val = _encoder->readEncoder();
  if (val != _lastEncoderValue) {
    _position         = val;
    _lastEncoderValue = val;
    _needsRedraw      = true;

    // ── Event emission ──
    float t = (float)_position / (float)MAX_POSITION;
    uint32_t tick = ticks::now();

    event_bus::emitOutputThrottled(nxyz_protocol::MODULE_CHLADNI, chladni_events::MODE_AMPLITUDE, t, tick);

    float centerVal = sampleCenterValue(t);
    bool  centerPositive = centerVal >= 0.0f;
    if (centerPositive != _prevCenterPositive) {
      // Chladni has no time-based velocity/intensity concept, unlike
      // collisions or pendulum swings — send a fixed max intensity.
      event_bus::emit(nxyz_protocol::MODULE_CHLADNI, chladni_events::NODE_CROSSING, 1.0f, 0.f, tick);
    }
    _prevCenterPositive = centerPositive;
  }

  // ── Redraw ──
  if (_needsRedraw) {
    uint32_t t0 = millis();
    drawPattern();
    Serial.printf("Chladni render: %ums  pos=%d\n",
      millis() - t0, _position);
  }

  // ── FPS ──
  static uint32_t frameCount = 0;
  static uint32_t lastFPS    = 0;
  frameCount++;
  uint32_t now_ms = millis();
  if (now_ms - lastFPS >= 1000) {
    _lastFPSVal = frameCount;
    _fpsDirty   = true;
    frameCount  = 0;
    lastFPS     = now_ms;
  }

  if (_fpsDirty) {
    drawFPS(_lastFPSVal);
    _fpsDirty = false;
  }
}