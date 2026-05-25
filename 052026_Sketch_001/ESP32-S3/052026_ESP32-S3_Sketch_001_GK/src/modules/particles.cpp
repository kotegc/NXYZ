#include "particles.h"

// ── Init / Stop ───────────────────────────────────────────
void ParticleModule::init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) {
  _gfx     = gfx;
  _encoder = encoder;

  _numBodies    = 0;
  _targetBodies = 0;
  _lastDrawnCount  = -1;
  _fpsDirty        = false;
  _lastEncoderValue = 0;
  _lastEncoderTime  = 0;

  memset(_bodies, 0, sizeof(_bodies));

  buildSprites();

  _gfx->fillScreen(TFT_WHITE);

  _encoder->setBoundaries(0, MAX_BODIES, false);
  _encoder->setEncoderValue(0);
  _encoder->setAcceleration(0);

  _mutex = xSemaphoreCreateMutex();

  Serial2.begin(31250, SERIAL_8N1, -1, 17); // RX=-1, TX=GPIO17

  xTaskCreatePinnedToCore(
    physicsTaskFn, "particles_physics",
    8192, this,
    2, &_physicsTask,
    0
  );

  drawCounter(0);
  drawFPS(0);
}

void ParticleModule::stop() {
  if (_physicsTask) {
    vTaskDelete(_physicsTask);
    _physicsTask = nullptr;
  }
  if (_mutex) {
    vSemaphoreDelete(_mutex);
    _mutex = nullptr;
  }
  _numBodies    = 0;
  _targetBodies = 0;
}

// ── Sprites ───────────────────────────────────────────────
void ParticleModule::buildSprites() {
  for (int y = 0; y < SPRITE_SIZE; y++) {
    for (int x = 0; x < SPRITE_SIZE; x++) {
      int dx = x - RADIUS;
      int dy = y - RADIUS;
      bool inside = (dx*dx + dy*dy) <= (RADIUS*RADIUS);
      _spriteWhite[y * SPRITE_SIZE + x] = inside ? 0x0000 : 0xFFFF;
      _spriteBlack[y * SPRITE_SIZE + x] = 0xFFFF;
    }
  }
}

// ── Body helpers ──────────────────────────────────────────
void ParticleModule::spawnBody(int i) {
  float vx = random(-30, 30) / 10.0f;
  float vy = random(-30, 30) / 10.0f;
  if (fabsf(vx) < 1.0f) vx = (vx >= 0) ? 1.5f : -1.5f;
  if (fabsf(vy) < 1.0f) vy = (vy >= 0) ? 1.5f : -1.5f;

  _bodies[i] = {
    (float)random(RADIUS+1, SCREEN_W-RADIUS-1),
    (float)random(RADIUS+1, SCREEN_H-RADIUS-1),
    vx, vy,
    0, 0, true
  };
  _bodies[i].px = (int)_bodies[i].x;
  _bodies[i].py = (int)_bodies[i].y;
}

void ParticleModule::killBody(int i) {
  _gfx->startWrite();
  _gfx->setAddrWindow(_bodies[i].px - RADIUS, _bodies[i].py - RADIUS, SPRITE_SIZE, SPRITE_SIZE);
  _gfx->writePixels(_spriteBlack, SPRITE_SIZE * SPRITE_SIZE);
  _gfx->endWrite();
  _bodies[i].alive = false;
}

void ParticleModule::killAllBodies() {
  for (int i = 0; i < MAX_BODIES; i++) {
    if (_bodies[i].alive) killBody(i);
  }
  _numBodies    = 0;
  _targetBodies = 0;
}

// ── Overlays ─────────────────────────────────────────────
void ParticleModule::drawCounter(int count) {
  _gfx->startWrite();
_gfx->fillRect(COUNTER_X1, COUNTER_Y1, 
               COUNTER_X2-COUNTER_X1, COUNTER_Y2-COUNTER_Y1, TFT_WHITE);
  _gfx->setTextColor(TFT_BLACK);
  _gfx->setTextSize(2);
  _gfx->setCursor(2, 2);
  _gfx->printf("N:%3d", count);
  _gfx->endWrite();
}

void ParticleModule::drawFPS(uint32_t fps) {
  _gfx->startWrite();
  _gfx->fillRect(FPS_X1, FPS_Y1,
                 FPS_X2-FPS_X1, FPS_Y2-FPS_Y1, TFT_WHITE);
  _gfx->setTextColor(TFT_BLACK);
  _gfx->setTextSize(2);
  _gfx->setCursor(FPS_X1 + 2, 2);
  _gfx->printf("FPS:%3u", fps);
  _gfx->endWrite();
}

// ── Physics task ──────────────────────────────────────────
void ParticleModule::physicsTaskFn(void* param) {
  ParticleModule* self = (ParticleModule*)param;
  const TickType_t interval = pdMS_TO_TICKS(5);
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    xSemaphoreTake(self->_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_BODIES; i++) {
      if (!self->_bodies[i].alive) continue;
      Body &b = self->_bodies[i];

      b.x += b.vx;
      b.y += b.vy;

      if (b.x + RADIUS >= SCREEN_W) { b.x = SCREEN_W-RADIUS; b.vx = -fabsf(b.vx); 
          Serial2.write((uint8_t)min((int)sqrtf(b.vx*b.vx + b.vy*b.vy) * 20, 255)); }
      if (b.x - RADIUS <= 0)        { b.x = RADIUS;           b.vx =  fabsf(b.vx); 
          Serial2.write((uint8_t)min((int)sqrtf(b.vx*b.vx + b.vy*b.vy) * 20, 255)); }
      if (b.y + RADIUS >= SCREEN_H) { b.y = SCREEN_H-RADIUS;  b.vy = -fabsf(b.vy); 
          Serial2.write((uint8_t)min((int)sqrtf(b.vx*b.vx + b.vy*b.vy) * 20, 255)); }
      if (b.y - RADIUS <= 0)        { b.y = RADIUS;            b.vy =  fabsf(b.vy); 
          Serial2.write((uint8_t)min((int)sqrtf(b.vx*b.vx + b.vy*b.vy) * 20, 255)); }

      for (int j = i+1; j < MAX_BODIES; j++) {
        if (!self->_bodies[j].alive) continue;

        float dx     = self->_bodies[j].x - b.x;
        float dy     = self->_bodies[j].y - b.y;
        float distSq = dx*dx + dy*dy;
        float minD   = (float)DIAMETER;

        if (distSq < minD*minD && distSq > 0.0001f) {
          float dist    = sqrtf(distSq);
          float nx      = dx/dist;
          float ny      = dy/dist;
          float overlap = (minD-dist)*0.5f;

          b.x                -= nx*overlap;
          b.y                -= ny*overlap;
          self->_bodies[j].x += nx*overlap;
          self->_bodies[j].y += ny*overlap;

          float dvx = b.vx - self->_bodies[j].vx;
          float dvy = b.vy - self->_bodies[j].vy;
          float dot  = dvx*nx + dvy*ny;

          if (dot > 0) {
              b.vx                -= dot*nx;
              b.vy                -= dot*ny;
              self->_bodies[j].vx += dot*nx;
              self->_bodies[j].vy += dot*ny;
              Serial2.write((uint8_t)min((int)sqrtf(dot*dot) * 20, 255));
          }
        }
      }
    }

    xSemaphoreGive(self->_mutex);
    vTaskDelayUntil(&lastWake, interval);
  }
}

// ── Loop (called from main) ───────────────────────────────
void ParticleModule::loop() {

  // ── Encoder ──
  if (_encoder->encoderChanged()) {
    int val = _encoder->readEncoder();
    uint32_t now_enc = millis();
    if (now_enc - _lastEncoderTime > 150) {
      _targetBodies     = val;
      _lastEncoderValue = val;
      _lastEncoderTime  = now_enc;
    } else {
      _encoder->setEncoderValue(_lastEncoderValue);
    }
  }

  // ── Button: killswitch ──
  if (_encoder->isEncoderButtonClicked()) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    killAllBodies();
    xSemaphoreGive(_mutex);
    _encoder->reset();
    _targetBodies = 0;
  }

  // ── Seed / kill one body per frame ──
  xSemaphoreTake(_mutex, portMAX_DELAY);

  if (_numBodies < _targetBodies) {
    for (int i = 0; i < MAX_BODIES && _numBodies < _targetBodies; i++) {
      if (!_bodies[i].alive) { spawnBody(i); _numBodies++; break; }
    }
  } else if (_numBodies > _targetBodies) {
    for (int i = MAX_BODIES-1; i >= 0 && _numBodies > _targetBodies; i--) {
      if (_bodies[i].alive) { killBody(i); _numBodies--; break; }
    }
  }

  Body snapshot[MAX_BODIES];
  int  snapshotCount = 0;
  int  snapshotIdx[MAX_BODIES];

  for (int i = 0; i < MAX_BODIES; i++) {
    if (_bodies[i].alive) {
      snapshot[snapshotCount]    = _bodies[i];
      snapshotIdx[snapshotCount] = i;
      snapshotCount++;
    }
  }
  xSemaphoreGive(_mutex);

  // ── Render ──
  bool counterDirty = (_numBodies != _lastDrawnCount);

  _gfx->startWrite();

  for (int s = 0; s < snapshotCount; s++) {
    int i  = snapshotIdx[s];
    int nx = (int)snapshot[s].x;
    int ny = (int)snapshot[s].y;

    if (nx != _bodies[i].px || ny != _bodies[i].py) {
      _gfx->setAddrWindow(_bodies[i].px-RADIUS, _bodies[i].py-RADIUS, SPRITE_SIZE, SPRITE_SIZE);
      _gfx->writePixels(_spriteBlack, SPRITE_SIZE*SPRITE_SIZE);
      _gfx->setAddrWindow(nx-RADIUS, ny-RADIUS, SPRITE_SIZE, SPRITE_SIZE);
      _gfx->writePixels(_spriteWhite, SPRITE_SIZE*SPRITE_SIZE);

      auto touchesRegion = [](int cx, int cy, int x1, int y1, int x2, int y2) {
        return (cx+RADIUS > x1 && cx-RADIUS < x2 &&
                cy+RADIUS > y1 && cy-RADIUS < y2);
      };

      if (!counterDirty) {
        if (touchesRegion(_bodies[i].px, _bodies[i].py, COUNTER_X1, COUNTER_Y1, COUNTER_X2, COUNTER_Y2) ||
            touchesRegion(nx, ny, COUNTER_X1, COUNTER_Y1, COUNTER_X2, COUNTER_Y2))
          counterDirty = true;
      }
      if (!_fpsDirty) {
        if (touchesRegion(_bodies[i].px, _bodies[i].py, FPS_X1, FPS_Y1, FPS_X2, FPS_Y2) ||
            touchesRegion(nx, ny, FPS_X1, FPS_Y1, FPS_X2, FPS_Y2))
          _fpsDirty = true;
      }

      _bodies[i].px = nx;
      _bodies[i].py = ny;
    }
  }

  _gfx->endWrite();

  if (counterDirty) {
    drawCounter(_numBodies);
    _lastDrawnCount = _numBodies;
  }
  if (_fpsDirty) {
    drawFPS(_lastFPSVal);
    _fpsDirty = false;
  }

  // ── FPS ──
  static uint32_t frameCount = 0;
  static uint32_t lastFPS    = 0;
  if (snapshotCount > 0) frameCount++;
  uint32_t now_ms = millis();
  if (now_ms - lastFPS >= 1000) {
    _lastFPSVal = frameCount;
    _fpsDirty   = true;
    Serial.printf("FPS:%u Bodies:%d\n", frameCount, _numBodies);
    frameCount  = 0;
    lastFPS     = now_ms;
  }
}