#include "daisy_seed.h"
#include "daisysp.h"

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
uint8_t packetBuf[6];
uint8_t packetIdx = 0;

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
void ProcessPacket(uint8_t* p)
{
    uint8_t bodyIndex     = p[1];
    float   speed         = p[2] / 254.0f;
    float   normX         = p[3] / 254.0f;
    float   normY         = p[4] / 254.0f;
    uint8_t collisionType = p[5];
    uint8_t numBodies     = p[6];

    // Currently mapped
    triggerFreq = 100.f + normX * 900.f;
    triggerAmp  = speed * 0.5f;

    // Parsed but unused for now — available for future mapping
    (void)bodyIndex;
    (void)normY;
    (void)collisionType;
    (void)numBodies;

    trigger = true;
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
    uart_conf.baudrate      = 31250;
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
            if(byte == 0xFF)
            {
                packetBuf[0] = 0xFF;
                packetIdx    = 1;
            }
            else if(packetIdx > 0 && packetIdx < 7)
            {
                packetBuf[packetIdx++] = byte;
                if(packetIdx == 7)
                {
                    ProcessPacket(packetBuf);
                    packetIdx = 0;
                }
            }
        }
    }
}