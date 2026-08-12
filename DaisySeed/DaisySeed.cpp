#include "daisy_seed.h"
#include "daisysp.h"
#include "uart_protocol.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hardware;
Encoder    enc;

// ── Voice pool ────────────────────────────────────────────
const int NUM_VOICES = 8;

struct Voice {
    Oscillator osc;
    AdEnv      env;
    bool       active = false;
};

Voice voices[NUM_VOICES];

// ── Shared state ──────────────────────────────────────────
bool    trigger    = false;
float   triggerFreq = 440.f;
float   triggerAmp  = 0.5f;

float   manualFreq = 440.f;

// Packet parser
uint8_t packetBuf[nxyz_protocol::PACKET_SIZE];
uint8_t packetIdx = 0;

// ── Event -> synth mapping table ───────────────────────────
// Data-driven replacement for the old single hardcoded normX->freq,
// speed->amp mapping: each row says "when this (moduleId, eventId) event
// arrives, drive this synth target from this source, scaled into this
// range." See 01_Docs/Architecture/event_protocol_overview.md.
enum class SynthParam : uint8_t { NoteTrigger, NoteFreq, NoteAmp };
enum class ValueSource : uint8_t { PacketValue, PacketAux, LatchedOutput };

struct FloatRange { float lo, hi; };

struct MappingEntry {
    uint8_t     moduleId;
    uint8_t     eventId;        // which incoming event this row reacts to
    SynthParam  target;
    FloatRange  range;          // scales the 0..1 source into this range (ignored for NoteTrigger)
    ValueSource source;
    uint8_t     sourceEventId;  // used only when source == LatchedOutput
};

static constexpr MappingEntry kMappingTable[] = {
    // Particles: aux carries collision X-position (pitch); value carries
    // normalized collision speed (amplitude). Reproduces pre-refactor
    // behavior exactly (100-1000Hz, 0-0.5 amp).
    { nxyz_protocol::MODULE_PARTICLES, 0 /*wall_bounce*/,    SynthParam::NoteTrigger, {0,0},          ValueSource::PacketValue,    0 },
    { nxyz_protocol::MODULE_PARTICLES, 0 /*wall_bounce*/,    SynthParam::NoteFreq,    {100.f,1000.f}, ValueSource::PacketAux,      0 },
    { nxyz_protocol::MODULE_PARTICLES, 0 /*wall_bounce*/,    SynthParam::NoteAmp,     {0.f,0.5f},     ValueSource::PacketValue,    0 },
    { nxyz_protocol::MODULE_PARTICLES, 1 /*body_collision*/, SynthParam::NoteTrigger, {0,0},          ValueSource::PacketValue,    0 },
    { nxyz_protocol::MODULE_PARTICLES, 1 /*body_collision*/, SynthParam::NoteFreq,    {100.f,1000.f}, ValueSource::PacketAux,      0 },
    { nxyz_protocol::MODULE_PARTICLES, 1 /*body_collision*/, SynthParam::NoteAmp,     {0.f,0.5f},     ValueSource::PacketValue,    0 },

    // Pendulum: continuous "angle" output (id 0) is latched and read as
    // pitch by the "peak" trigger (id 1) — pendulum has no drone voice,
    // so an Output alone can't drive audio yet.
    { nxyz_protocol::MODULE_PENDULUM, 1 /*peak*/, SynthParam::NoteTrigger, {0,0},         ValueSource::PacketValue,   0 },
    { nxyz_protocol::MODULE_PENDULUM, 1 /*peak*/, SynthParam::NoteFreq,    {200.f,800.f}, ValueSource::LatchedOutput, 0 /*angle*/ },
    { nxyz_protocol::MODULE_PENDULUM, 1 /*peak*/, SynthParam::NoteAmp,     {0.15f,0.45f}, ValueSource::PacketValue,   0 },

    // Chladni: same pattern — "mode_amplitude" (id 0) latched, read as
    // pitch by "node_crossing" (id 1).
    { nxyz_protocol::MODULE_CHLADNI, 1 /*node_crossing*/, SynthParam::NoteTrigger, {0,0},         ValueSource::PacketValue,   0 },
    { nxyz_protocol::MODULE_CHLADNI, 1 /*node_crossing*/, SynthParam::NoteFreq,    {150.f,900.f}, ValueSource::LatchedOutput, 0 /*mode_amplitude*/ },
    { nxyz_protocol::MODULE_CHLADNI, 1 /*node_crossing*/, SynthParam::NoteAmp,     {0.2f,0.4f},   ValueSource::PacketValue,   0 },
};

static constexpr uint8_t MAX_EVENTS_PER_MODULE = 4;
static float    g_latchedOutput[nxyz_protocol::MODULE_COUNT][MAX_EVENTS_PER_MODULE] = {};
static uint32_t g_lastEventTick = 0;

// ── Voice allocator ───────────────────────────────────────
int FindVoice()
{
    // First look for an inactive voice
    for(int i = 0; i < NUM_VOICES; i++)
    {
        if(!voices[i].active) return i;
    }

    // All busy — steal the quietest
    int   quietest  = 0;
    float minAmp    = 999.f;
    for(int i = 0; i < NUM_VOICES; i++)
    {
        float a = voices[i].env.Process();
        if(a < minAmp)
        {
            minAmp   = a;
            quietest = i;
        }
    }
    return quietest;
}

// ── Packet processor ──────────────────────────────────────
// Looks up every kMappingTable row matching this packet's (moduleId,
// eventId) and applies each in turn — see the table above for what each
// module's events actually drive.
void ApplyPacket(const nxyz_protocol::EventPacket& pkt)
{
    // Every incoming packet updates the latch for its own (moduleId,
    // eventId), regardless of kind — this is what lets a later trigger
    // read a continuous output's most recent value (e.g. pendulum's
    // "peak" reading the last "angle" sample for pitch).
    g_latchedOutput[pkt.moduleId][pkt.eventId] = pkt.value;
    g_lastEventTick = pkt.tick;

    bool  doTrigger = false;
    float freq = triggerFreq, amp = triggerAmp;

    for (const auto& m : kMappingTable) {
        if (m.moduleId != pkt.moduleId || m.eventId != pkt.eventId) continue;

        float src = 0.f;
        switch (m.source) {
            case ValueSource::PacketValue:   src = pkt.value; break;
            case ValueSource::PacketAux:     src = pkt.aux;   break;
            case ValueSource::LatchedOutput: src = g_latchedOutput[pkt.moduleId][m.sourceEventId]; break;
        }

        switch (m.target) {
            case SynthParam::NoteTrigger: doTrigger = true; break;
            case SynthParam::NoteFreq:    freq = m.range.lo + src * (m.range.hi - m.range.lo); break;
            case SynthParam::NoteAmp:     amp  = m.range.lo + src * (m.range.hi - m.range.lo); break;
        }
    }

    if (doTrigger) {
        triggerFreq = freq;
        triggerAmp  = amp;
        trigger     = true;
    }
}

// ── Audio callback ────────────────────────────────────────
void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    if(trigger)
    {
        int v = FindVoice();
        voices[v].osc.SetFreq(triggerFreq);
        voices[v].env.SetMax(triggerAmp);
        voices[v].env.Trigger();
        voices[v].active = true;
        trigger = false;
    }

    for(size_t i = 0; i < size; i += 2)
    {
        float mix = 0.f;
        int   activeCount = 0;

        for(int v = 0; v < NUM_VOICES; v++)
        {
            if(!voices[v].active) continue;

            float env_out = voices[v].env.Process();

            if(env_out <= 0.0001f)
            {
                voices[v].active = false;
                continue;
            }

            voices[v].osc.SetAmp(env_out);
            mix += voices[v].osc.Process();
            activeCount++;
        }

        // Scale down to prevent clipping
        if(activeCount > 0)
            mix /= activeCount;

        out[i]     = mix;
        out[i + 1] = mix;
    }
}

// ── Main ──────────────────────────────────────────────────
int main(void)
{
    hardware.Configure();
    hardware.Init();
    hardware.SetAudioBlockSize(4);

    float samplerate = hardware.AudioSampleRate();

    UartHandler uart;
    UartHandler::Config uart_conf;
    uart_conf.periph        = UartHandler::Config::Peripheral::USART_1;
    uart_conf.baudrate      = nxyz_protocol::BAUD_RATE;
    uart_conf.stopbits      = UartHandler::Config::StopBits::BITS_1;
    uart_conf.parity        = UartHandler::Config::Parity::NONE;
    uart_conf.mode          = UartHandler::Config::Mode::RX;
    uart_conf.pin_config.rx = seed::D14;
    uart_conf.pin_config.tx = seed::D13;
    uart.Init(uart_conf);

    enc.Init(seed::D19, seed::D20, seed::D21);

    // Init all voices
    for(int i = 0; i < NUM_VOICES; i++)
    {
        voices[i].osc.Init(samplerate);
        voices[i].osc.SetWaveform(Oscillator::WAVE_SIN);
        voices[i].osc.SetAmp(1.f);
        voices[i].osc.SetFreq(440.f);

        voices[i].env.Init(samplerate);
        voices[i].env.SetTime(ADENV_SEG_ATTACK, .01f);
        voices[i].env.SetTime(ADENV_SEG_DECAY, .4f);
        voices[i].env.SetMin(0.0f);
        voices[i].env.SetMax(1.f);
        voices[i].env.SetCurve(0);

        voices[i].active = false;
    }

    hardware.StartAudio(AudioCallback);

    while(1)
    {
        enc.Debounce();

        // Encoder controls manual frequency
        int inc = enc.Increment();
        if(inc != 0)
        {
            manualFreq += inc * 10.f;
            manualFreq = fclamp(manualFreq, 20.f, 20000.f);
        }

        // Encoder click triggers a manual voice at current frequency
        if(enc.RisingEdge())
        {
            triggerFreq = manualFreq;
            triggerAmp  = 0.5f;
            trigger     = true;
        }

        // Parse UART packets
        uint8_t byte;
        while(uart.PollReceive(&byte, 1, 0) == 0)
        {
            if(byte == nxyz_protocol::SYNC_BYTE)
            {
                packetBuf[0] = nxyz_protocol::SYNC_BYTE;
                packetIdx    = 1;
            }
            else if(packetIdx > 0 && packetIdx < nxyz_protocol::PACKET_SIZE)
            {
                packetBuf[packetIdx++] = byte;
                if(packetIdx == nxyz_protocol::PACKET_SIZE)
                {
                    ApplyPacket(nxyz_protocol::decodeEventPacket(packetBuf));
                    packetIdx = 0;
                }
            }
        }
    }
}