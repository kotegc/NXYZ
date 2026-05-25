#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hardware;
Oscillator osc;
Encoder    enc;
AdEnv      env;

float freq       = 440.f;
bool  trigger    = false;
float triggerAmp = 1.0f;

// Packet parser state
uint8_t packetBuf[6];
uint8_t packetIdx = 0;

void ProcessPacket(uint8_t* p)
{
    uint8_t bodyIndex     = p[1];
    float   speed         = p[2] / 254.0f;
    float   normX         = p[3] / 254.0f;
    float   normY         = p[4] / 254.0f;
    uint8_t collisionType = p[5];

    // Map normX to frequency (left=low, right=high)
    freq = 100.f + normX * 900.f; // 100Hz to 1000Hz
    osc.SetFreq(freq);

    // Speed controls amplitude
    triggerAmp = speed * 0.5f; // cap at 50% for earbud safety

    trigger = true;
}

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    if(trigger)
    {
        env.SetMax(triggerAmp);
        env.Trigger();
        trigger = false;
    }

    for(size_t i = 0; i < size; i += 2)
    {
        float env_out = env.Process();
        osc.SetAmp(env_out);
        float sig = osc.Process();
        out[i]     = sig;
        out[i + 1] = sig;
    }
}

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

    osc.Init(samplerate);
    osc.SetWaveform(osc.WAVE_SIN);
    osc.SetAmp(1.f);
    osc.SetFreq(freq);

    env.Init(samplerate);
    env.SetTime(ADENV_SEG_ATTACK, .01);
    env.SetTime(ADENV_SEG_DECAY, .4);
    env.SetMin(0.0);
    env.SetMax(1.f);
    env.SetCurve(0);

    hardware.StartAudio(AudioCallback);

    while(1)
    {
        enc.Debounce();

        // Encoder still controls frequency manually
        int inc = enc.Increment();
        if(inc != 0)
        {
            freq += inc * 10.f;
            freq = fclamp(freq, 20.f, 20000.f);
            osc.SetFreq(freq);
        }

        if(enc.RisingEdge())
        {
            triggerAmp = 0.5f;
            trigger = true;
        }

        // Parse incoming UART packets
        uint8_t byte;
        while(uart.PollReceive(&byte, 1, 0) == 0)
        {
            if(byte == 0xFF)
            {
                // Header found — start new packet
                packetBuf[0] = 0xFF;
                packetIdx    = 1;
            }
            else if(packetIdx > 0 && packetIdx < 6)
            {
                packetBuf[packetIdx++] = byte;
                if(packetIdx == 6)
                {
                    ProcessPacket(packetBuf);
                    packetIdx = 0;
                }
            }
        }
    }
}