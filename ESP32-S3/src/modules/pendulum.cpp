#include "pendulum.h"
#include <math.h>
#include <algorithm>

using std::min;
using std::max;

static void computeDerivatives(
    double a1, double a2, double v1, double v2,
    double& da1, double& da2, double& dv1, double& dv2,
    float m1, float m2, float g, float l1, float l2)
{
  double da   = a2 - a1;
  double den1 = (m1 + m2) * l1 - m2 * l1 * cos(da) * cos(da);
  double den2 = (l2 / l1) * den1;

  dv1 = (m2 * l1 * v1 * v1 * sin(da) * cos(da)
       + m2 * g  * sin(a2) * cos(da)
       + m2 * l2 * v2 * v2 * sin(da)
       - (m1 + m2) * g * sin(a1)) / den1;

  dv2 = (-m2 * l2 * v2 * v2 * sin(da) * cos(da)
       + (m1 + m2) * g * sin(a1) * cos(da)
       - (m1 + m2) * l1 * v1 * v1 * sin(da)
       - (m1 + m2) * g * sin(a2)) / den2;

  da1 = v1;
  da2 = v2;
}

// ── Trail storage ─────────────────────────────────────────
static constexpr int MAX_TRAIL = 3000;
static int trail1X[MAX_TRAIL];
static int trail1Y[MAX_TRAIL];
static int trail2X[MAX_TRAIL];
static int trail2Y[MAX_TRAIL];
static int trailCount = 0;

// ── Init / Stop ───────────────────────────────────────────
void PendulumModule::init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) {
  _gfx        = gfx;
  _encoder    = encoder;
  _running    = false;
  _firstFrame = true;
  _fpsDirty   = false;
  _lastFPSVal = 0;
  trailCount  = 0;

  _encoder->setBoundaries(-100000, 100000, false);
  _encoder->setEncoderValue(0);
  _encoder->setAcceleration(0);
  _lastEncoderRaw = 0;

  _a1 = 0.8;
  _a2 = 0.8;
  _v1 = 0;
  _v2 = 0;

  _gfx->fillScreen(TFT_WHITE);

  computePositions();
  drawAll();
  drawFPS(0);
}

void PendulumModule::stop() {
  _running   = false;
  trailCount = 0;
}

// ── Position compute ──────────────────────────────────────
void PendulumModule::computePositions() {
  _x1 = PIVOT_X + (int)(L1 * sin(_a1));
  _y1 = PIVOT_Y + (int)(L1 * cos(_a1));
  _x2 = _x1    + (int)(L2 * sin(_a2));
  _y2 = _y1    + (int)(L2 * cos(_a2));
}

// ── Sprite render ─────────────────────────────────────────
void PendulumModule::renderSprite(
    int oldX1, int oldY1, int oldX2, int oldY2,
    int newX1, int newY1, int newX2, int newY2)
{
  // Compute bounding box covering old and new positions
  int x0 = min({PIVOT_X, oldX1, oldX2, newX1, newX2}) - ANCHOR_RADIUS - 2;
  int y0 = min({PIVOT_Y, oldY1, oldY2, newY1, newY2}) - ANCHOR_RADIUS - 2;
  int x1 = max({PIVOT_X, oldX1, oldX2, newX1, newX2}) + ANCHOR_RADIUS + 2;
  int y1 = max({PIVOT_Y, oldY1, oldY2, newY1, newY2}) + ANCHOR_RADIUS + 2;

  // Clamp to screen
  x0 = max(x0, 0);
  y0 = max(y0, 0);
  x1 = min(x1, SCREEN_W - 1);
  y1 = min(y1, SCREEN_H - 1);

  int sw = x1 - x0 + 1;
  int sh = y1 - y0 + 1;

  // Create sprite
  LGFX_Sprite sprite(_gfx);
  sprite.setColorDepth(16);
  sprite.createSprite(sw, sh);
  sprite.fillSprite(TFT_WHITE);

  // Draw trail pixels that fall inside this region
  for (int t = 0; t < trailCount; t++) {
    if (trail1X[t] >= x0 && trail1X[t] <= x1 &&
        trail1Y[t] >= y0 && trail1Y[t] <= y1) {
      sprite.drawPixel(trail1X[t] - x0, trail1Y[t] - y0, COLOR_ACCENT1);
    }
    if (trail2X[t] >= x0 && trail2X[t] <= x1 &&
        trail2Y[t] >= y0 && trail2Y[t] <= y1) {
      sprite.drawPixel(trail2X[t] - x0, trail2Y[t] - y0, COLOR_ACCENT2);
    }
  }

  // Draw arms into sprite (offset by x0, y0)
  sprite.drawLine(PIVOT_X - x0, PIVOT_Y - y0, newX1 - x0, newY1 - y0, TFT_BLACK);
  sprite.drawLine(PIVOT_X - x0 + 1, PIVOT_Y - y0, newX1 - x0 + 1, newY1 - y0, TFT_BLACK);
  sprite.drawLine(newX1 - x0, newY1 - y0, newX2 - x0, newY2 - y0, TFT_BLACK);
  sprite.drawLine(newX1 - x0 + 1, newY1 - y0, newX2 - x0 + 1, newY2 - y0, TFT_BLACK);

  // Draw bobs
  sprite.fillCircle(newX1 - x0, newY1 - y0, BOB_RADIUS, COLOR_ACCENT1);
  sprite.fillCircle(newX2 - x0, newY2 - y0, BOB_RADIUS, COLOR_ACCENT2);

  // Draw anchor
  sprite.fillCircle(PIVOT_X - x0, PIVOT_Y - y0, ANCHOR_RADIUS, TFT_BLACK);

  // Push sprite to display in one transfer
  sprite.pushSprite(x0, y0);
  sprite.deleteSprite();
}

// ── drawAll / eraseAll for setup mode ────────────────────
void PendulumModule::drawAll() {
  _gfx->drawLine(PIVOT_X,   PIVOT_Y, _x1,   _y1, TFT_BLACK);
  _gfx->drawLine(PIVOT_X+1, PIVOT_Y, _x1+1, _y1, TFT_BLACK);
  _gfx->drawLine(_x1,   _y1, _x2,   _y2, TFT_BLACK);
  _gfx->drawLine(_x1+1, _y1, _x2+1, _y2, TFT_BLACK);
  _gfx->fillCircle(_x1, _y1, BOB_RADIUS, COLOR_ACCENT1);
  _gfx->fillCircle(_x2, _y2, BOB_RADIUS, COLOR_ACCENT2);
  _gfx->fillCircle(PIVOT_X, PIVOT_Y, ANCHOR_RADIUS, TFT_BLACK);
}

void PendulumModule::eraseAll() {
  _gfx->drawLine(PIVOT_X,   PIVOT_Y, _x1,   _y1, TFT_WHITE);
  _gfx->drawLine(PIVOT_X+1, PIVOT_Y, _x1+1, _y1, TFT_WHITE);
  _gfx->drawLine(_x1,   _y1, _x2,   _y2, TFT_WHITE);
  _gfx->drawLine(_x1+1, _y1, _x2+1, _y2, TFT_WHITE);
  _gfx->fillCircle(_x1, _y1, BOB_RADIUS, TFT_WHITE);
  _gfx->fillCircle(_x2, _y2, BOB_RADIUS, TFT_WHITE);
  _gfx->fillCircle(PIVOT_X, PIVOT_Y, ANCHOR_RADIUS, TFT_WHITE);
}

void PendulumModule::redrawTrail(int x0, int y0, int x1, int y1) {
  for (int t = 0; t < trailCount; t++) {
    if (trail1X[t] >= x0 && trail1X[t] <= x1 &&
        trail1Y[t] >= y0 && trail1Y[t] <= y1) {
      _gfx->drawPixel(trail1X[t], trail1Y[t], COLOR_ACCENT1);
    }
    if (trail2X[t] >= x0 && trail2X[t] <= x1 &&
        trail2Y[t] >= y0 && trail2Y[t] <= y1) {
      _gfx->drawPixel(trail2X[t], trail2Y[t], COLOR_ACCENT2);
    }
  }
}

// ── FPS overlay ───────────────────────────────────────────
void PendulumModule::drawFPS(uint32_t fps) {
  _gfx->startWrite();
  _gfx->fillRect(FPS_X1, FPS_Y1, FPS_X2 - FPS_X1, FPS_Y2 - FPS_Y1, TFT_WHITE);
  _gfx->setTextColor(TFT_BLACK);
  _gfx->setTextSize(2);
  _gfx->setCursor(FPS_X1 + 2, 2);
  _gfx->printf("FPS:%3u", fps);
  _gfx->endWrite();
}

// ── Physics ───────────────────────────────────────────────
void PendulumModule::stepPhysics() {
  double da1, da2, dv1, dv2;

  computeDerivatives(_a1, _a2, _v1, _v2, da1, da2, dv1, dv2, M1, M2, G, L1, L2);
  double k1_a1=da1, k1_a2=da2, k1_v1=dv1, k1_v2=dv2;

  computeDerivatives(_a1+k1_a1*DT/2, _a2+k1_a2*DT/2,
                     _v1+k1_v1*DT/2, _v2+k1_v2*DT/2,
                     da1, da2, dv1, dv2, M1, M2, G, L1, L2);
  double k2_a1=da1, k2_a2=da2, k2_v1=dv1, k2_v2=dv2;

  computeDerivatives(_a1+k2_a1*DT/2, _a2+k2_a2*DT/2,
                     _v1+k2_v1*DT/2, _v2+k2_v2*DT/2,
                     da1, da2, dv1, dv2, M1, M2, G, L1, L2);
  double k3_a1=da1, k3_a2=da2, k3_v1=dv1, k3_v2=dv2;

  computeDerivatives(_a1+k3_a1*DT, _a2+k3_a2*DT,
                     _v1+k3_v1*DT, _v2+k3_v2*DT,
                     da1, da2, dv1, dv2, M1, M2, G, L1, L2);
  double k4_a1=da1, k4_a2=da2, k4_v1=dv1, k4_v2=dv2;

  _a1 += (DT/6.0)*(k1_a1 + 2*k2_a1 + 2*k3_a1 + k4_a1);
  _a2 += (DT/6.0)*(k1_a2 + 2*k2_a2 + 2*k3_a2 + k4_a2);
  _v1 += (DT/6.0)*(k1_v1 + 2*k2_v1 + 2*k3_v1 + k4_v1);
  _v2 += (DT/6.0)*(k1_v2 + 2*k2_v2 + 2*k3_v2 + k4_v2);
}

// ── Main loop ─────────────────────────────────────────────
void PendulumModule::loop() {

  // ── Setup mode ──
  if (!_running) {
    if (_encoder->encoderChanged()) {
      long raw   = _encoder->readEncoder();
      long delta = raw - _lastEncoderRaw;
      _lastEncoderRaw = raw;

      int oldX1 = _x1, oldY1 = _y1;
      int oldX2 = _x2, oldY2 = _y2;

      _a1 -= delta * 0.4188; // empirically-tuned encoder-tick-to-radians sensitivity, not derived
      _a2  = _a1;
      computePositions();

      renderSprite(oldX1, oldY1, oldX2, oldY2,
                   _x1,   _y1,   _x2,   _y2);
      drawFPS(_lastFPSVal);
    }

    if (_encoder->isEncoderButtonClicked()) {
      _v1         = 0;
      _v2         = 0;
      _running    = true;
      _firstFrame = true;
      trailCount  = 0;
    }
    return;
  }

  // ── Reset on button press ──
  if (_encoder->isEncoderButtonClicked()) {
    _running    = false;
    _firstFrame = true;
    _v1 = 0; _v2 = 0;
    _lastEncoderRaw = _encoder->readEncoder();
    _a2 = _a1;
    trailCount = 0;
    _gfx->fillScreen(TFT_WHITE);
    computePositions();
    drawAll();
    drawFPS(_lastFPSVal);
    return;
  }

  // ── Physics + render ──
  stepPhysics();

  int oldX1 = _x1, oldY1 = _y1;
  int oldX2 = _x2, oldY2 = _y2;

  computePositions();

  // Render via sprite — no blink
  renderSprite(oldX1, oldY1, oldX2, oldY2,
               _x1,   _y1,   _x2,   _y2);

  // Append to trail
  if (trailCount < MAX_TRAIL) {
    trail1X[trailCount] = _x1;
    trail1Y[trailCount] = _y1;
    trail2X[trailCount] = _x2;
    trail2Y[trailCount] = _y2;
    trailCount++;
  }

  _firstFrame = false;

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