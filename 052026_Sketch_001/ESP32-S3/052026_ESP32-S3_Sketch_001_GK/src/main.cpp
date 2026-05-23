#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <AiEsp32RotaryEncoder.h>

// ── LovyanGFX config ─────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk   = 12;
      cfg.pin_mosi   = 11;
      cfg.pin_miso   = 13;
      cfg.pin_dc     = 8;
      cfg.dma_channel = SPI_DMA_CH_AUTO;  // ← this is the key line
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 10;
      cfg.pin_rst  = 9;
      cfg.pin_busy = -1;
      cfg.memory_width  = 240;
      cfg.memory_height = 320;
      cfg.panel_width   = 240;
      cfg.panel_height  = 320;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = true;
      cfg.invert          = false;
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

LGFX gfx;

// ── Display dimensions ────────────────────────────────────
#define SCREEN_W  320
#define SCREEN_H  240

// ── Overlay regions ───────────────────────────────────────
#define COUNTER_X1  0
#define COUNTER_Y1  0
#define COUNTER_X2  72
#define COUNTER_Y2  18

#define FPS_X1  (SCREEN_W - 140)
#define FPS_Y1  0
#define FPS_X2  SCREEN_W
#define FPS_Y2  18

// ── Encoder ───────────────────────────────────────────────
#define ENC_CLK   4
#define ENC_DT    5
#define ENC_SW    6
#define ENC_STEPS 4

AiEsp32RotaryEncoder rotaryEncoder =
    AiEsp32RotaryEncoder(ENC_DT, ENC_CLK, ENC_SW, -1, ENC_STEPS);

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// ── Bodies ────────────────────────────────────────────────
#define MAX_BODIES  100
#define RADIUS        8
#define DIAMETER     (RADIUS * 2)

const float BOUNCE = 1.0f;
const float DAMPEN = 1.0f;

struct Body {
  float x, y;
  float vx, vy;
  int   px, py;
  bool  alive;
};

Body bodies[MAX_BODIES];
int  numBodies    = 0;
int  targetBodies = 0;

SemaphoreHandle_t bodyMutex;

// ── Sprites ───────────────────────────────────────────────
#define SPRITE_SIZE (RADIUS * 2 + 1)

uint16_t spriteWhite[SPRITE_SIZE * SPRITE_SIZE];
uint16_t spriteBlack[SPRITE_SIZE * SPRITE_SIZE];

void buildSprites() {
  for (int y = 0; y < SPRITE_SIZE; y++) {
    for (int x = 0; x < SPRITE_SIZE; x++) {
      int dx = x - RADIUS;
      int dy = y - RADIUS;
      bool inside = (dx * dx + dy * dy) <= (RADIUS * RADIUS);
      spriteWhite[y * SPRITE_SIZE + x] = inside ? 0xFFFF : 0x0000;
      spriteBlack[y * SPRITE_SIZE + x] = 0x0000;
    }
  }
}

// ── Body helpers ──────────────────────────────────────────
void spawnBody(int i) {
  float vx = random(-30, 30) / 10.0f;
  float vy = random(-30, 30) / 10.0f;
  if (fabsf(vx) < 1.0f) vx = (vx >= 0) ? 1.5f : -1.5f;
  if (fabsf(vy) < 1.0f) vy = (vy >= 0) ? 1.5f : -1.5f;

  bodies[i].x     = random(RADIUS + 1, SCREEN_W - RADIUS - 1);
  bodies[i].y     = random(RADIUS + 1, SCREEN_H - RADIUS - 1);
  bodies[i].vx    = vx;
  bodies[i].vy    = vy;
  bodies[i].px    = (int)bodies[i].x;
  bodies[i].py    = (int)bodies[i].y;
  bodies[i].alive = true;
}

void killBody(int i) {
  gfx.startWrite();
  gfx.setAddrWindow(bodies[i].px - RADIUS, bodies[i].py - RADIUS, SPRITE_SIZE, SPRITE_SIZE);
  gfx.writePixels(spriteBlack, SPRITE_SIZE * SPRITE_SIZE);
  gfx.endWrite();
  bodies[i].alive = false;
}

void killAllBodies() {
  for (int i = 0; i < MAX_BODIES; i++) {
    if (bodies[i].alive) killBody(i);
  }
  numBodies    = 0;
  targetBodies = 0;
}

// ── Physics task (Core 0) ─────────────────────────────────
void physicsTask(void *param) {
  const TickType_t interval = pdMS_TO_TICKS(5);
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    xSemaphoreTake(bodyMutex, portMAX_DELAY);

    for (int i = 0; i < MAX_BODIES; i++) {
      if (!bodies[i].alive) continue;
      Body &b = bodies[i];

      b.x += b.vx;
      b.y += b.vy;

      if (b.x + RADIUS >= SCREEN_W) { b.x = SCREEN_W - RADIUS; b.vx = -fabsf(b.vx); }
      if (b.x - RADIUS <= 0)        { b.x = RADIUS;             b.vx =  fabsf(b.vx); }
      if (b.y + RADIUS >= SCREEN_H) { b.y = SCREEN_H - RADIUS;  b.vy = -fabsf(b.vy); }
      if (b.y - RADIUS <= 0)        { b.y = RADIUS;             b.vy =  fabsf(b.vy); }

      for (int j = i + 1; j < MAX_BODIES; j++) {
        if (!bodies[j].alive) continue;

        float dx     = bodies[j].x - b.x;
        float dy     = bodies[j].y - b.y;
        float distSq = dx * dx + dy * dy;
        float minD   = (float)DIAMETER;

        if (distSq < minD * minD && distSq > 0.0001f) {
          float dist    = sqrtf(distSq);
          float nx      = dx / dist;
          float ny      = dy / dist;
          float overlap = (minD - dist) * 0.5f;

          b.x         -= nx * overlap;
          b.y         -= ny * overlap;
          bodies[j].x += nx * overlap;
          bodies[j].y += ny * overlap;

          float dvx = b.vx - bodies[j].vx;
          float dvy = b.vy - bodies[j].vy;
          float dot  = dvx * nx + dvy * ny;

          if (dot > 0) {
            b.vx         -= dot * nx;
            b.vy         -= dot * ny;
            bodies[j].vx += dot * nx;
            bodies[j].vy += dot * ny;
          }
        }
      }
    }

    xSemaphoreGive(bodyMutex);
    vTaskDelayUntil(&lastWake, interval);
  }
}

// ── Overlays ──────────────────────────────────────────────
void drawCounter(int count) {
  gfx.startWrite();
  gfx.fillRect(COUNTER_X1, COUNTER_Y1,
               COUNTER_X2 - COUNTER_X1, COUNTER_Y2 - COUNTER_Y1, TFT_BLACK);
  gfx.setTextColor(TFT_WHITE);
  gfx.setTextSize(2);
  gfx.setCursor(2, 2);
  gfx.printf("N:%3d", count);
  gfx.endWrite();
}

void drawFPS(uint32_t fps) {
  gfx.startWrite();
  gfx.fillRect(FPS_X1, FPS_Y1,
               FPS_X2 - FPS_X1, FPS_Y2 - FPS_Y1, TFT_BLACK);
  gfx.setTextColor(TFT_WHITE);
  gfx.setTextSize(2);
  gfx.setCursor(FPS_X1 + 2, 2);
  gfx.printf("FPS:%3u", fps);
  gfx.endWrite();
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Boot OK");

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, MAX_BODIES, false);
  rotaryEncoder.setAcceleration(0);

  gfx.init();
  gfx.setRotation(1);
  gfx.fillScreen(TFT_BLACK);
  buildSprites();
  Serial.println("Display OK");

  randomSeed(analogRead(A0));
  bodyMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    physicsTask, "physics",
    8192, nullptr,
    2, nullptr,
    0
  );

  Serial.println("Physics task OK");
  drawCounter(0);
  drawFPS(0);

  Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

// ── Loop (Core 1) — render + input ───────────────────────
void loop() {
  static int      lastDrawnCount  = -1;
  static uint32_t lastFPSVal      = 0;
  static bool     fpsDirty        = false;

  // ── Encoder ──
  static int      lastEncoderValue = 0;
  static uint32_t lastEncoderTime  = 0;

  if (rotaryEncoder.encoderChanged()) {
    int val = rotaryEncoder.readEncoder();
    uint32_t now_enc = millis();
    if (now_enc - lastEncoderTime > 150) {
      targetBodies     = val;
      lastEncoderValue = val;
      lastEncoderTime  = now_enc;
    } else {
      rotaryEncoder.setEncoderValue(lastEncoderValue);
    }
  }

  // ── Button: killswitch ──
  if (rotaryEncoder.isEncoderButtonClicked()) {
    xSemaphoreTake(bodyMutex, portMAX_DELAY);
    killAllBodies();
    xSemaphoreGive(bodyMutex);
    rotaryEncoder.reset();
    targetBodies = 0;
  }

  // ── Seed / kill one body per frame ──
  xSemaphoreTake(bodyMutex, portMAX_DELAY);

  if (numBodies < targetBodies) {
    for (int i = 0; i < MAX_BODIES && numBodies < targetBodies; i++) {
      if (!bodies[i].alive) { spawnBody(i); numBodies++; break; }
    }
  } else if (numBodies > targetBodies) {
    for (int i = MAX_BODIES - 1; i >= 0 && numBodies > targetBodies; i--) {
      if (bodies[i].alive) { killBody(i); numBodies--; break; }
    }
  }

  Body snapshot[MAX_BODIES];
  int  snapshotCount = 0;
  int  snapshotIdx[MAX_BODIES];

  for (int i = 0; i < MAX_BODIES; i++) {
    if (bodies[i].alive) {
      snapshot[snapshotCount] = bodies[i];
      snapshotIdx[snapshotCount] = i;
      snapshotCount++;
    }
  }
  xSemaphoreGive(bodyMutex);

  // ── Render ──
  bool counterDirty = (numBodies != lastDrawnCount);

  gfx.startWrite();

  for (int s = 0; s < snapshotCount; s++) {
    int i  = snapshotIdx[s];

    int nx = (int)snapshot[s].x;
    int ny = (int)snapshot[s].y;

    if (nx != bodies[i].px || ny != bodies[i].py) {
      gfx.setAddrWindow(bodies[i].px - RADIUS, bodies[i].py - RADIUS, SPRITE_SIZE, SPRITE_SIZE);
      gfx.writePixels(spriteBlack, SPRITE_SIZE * SPRITE_SIZE);
      gfx.setAddrWindow(nx - RADIUS, ny - RADIUS, SPRITE_SIZE, SPRITE_SIZE);
      gfx.writePixels(spriteWhite, SPRITE_SIZE * SPRITE_SIZE);

      auto touchesRegion = [](int cx, int cy, int x1, int y1, int x2, int y2) {
        return (cx + RADIUS > x1 && cx - RADIUS < x2 &&
                cy + RADIUS > y1 && cy - RADIUS < y2);
      };

      if (!counterDirty) {
        if (touchesRegion(bodies[i].px, bodies[i].py, COUNTER_X1, COUNTER_Y1, COUNTER_X2, COUNTER_Y2) ||
            touchesRegion(nx, ny, COUNTER_X1, COUNTER_Y1, COUNTER_X2, COUNTER_Y2)) {
          counterDirty = true;
        }
      }

      if (!fpsDirty) {
        if (touchesRegion(bodies[i].px, bodies[i].py, FPS_X1, FPS_Y1, FPS_X2, FPS_Y2) ||
            touchesRegion(nx, ny, FPS_X1, FPS_Y1, FPS_X2, FPS_Y2)) {
          fpsDirty = true;
        }
      }

      bodies[i].px = nx;
      bodies[i].py = ny;
    }
  }

  gfx.endWrite();

  if (counterDirty) {
    drawCounter(numBodies);
    lastDrawnCount = numBodies;
  }

  if (fpsDirty) {
    drawFPS(lastFPSVal);
    fpsDirty = false;
  }

  // ── FPS ──
  static uint32_t frameCount = 0;
  static uint32_t lastFPS    = 0;

  if (snapshotCount > 0) frameCount++;  // only count frames where rendering happened

  uint32_t now_ms = millis();
  if (now_ms - lastFPS >= 1000) {
    lastFPSVal = frameCount;
    fpsDirty   = true;
    Serial.printf("FPS:%u Bodies:%d\n", frameCount, numBodies);
    frameCount = 0;
    lastFPS    = now_ms;
  }
}