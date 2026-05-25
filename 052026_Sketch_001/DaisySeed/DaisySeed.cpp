#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hardware;
Oscillator osc;
Encoder    enc;
AdEnv      env;

float freq    = 440.f;
bool  trigger = false;
float triggerAmp = 1.0f;

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

    // UART receive from ESP32
    UartHandler uart;
    UartHandler::Config uart_conf;
    uart_conf.periph        = UartHandler::Config::Peripheral::USART_1;
    uart_conf.baudrate      = 31250;
    uart_conf.stopbits      = UartHandler::Config::StopBits::BITS_1;
    uart_conf.parity        = UartHandler::Config::Parity::NONE;
    uart_conf.mode          = UartHandler::Config::Mode::RX;
    uart_conf.pin_config.rx = hardware.GetPin(14);
    uart_conf.pin_config.tx = hardware.GetPin(13);
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

        int inc = enc.Increment();
        if(inc != 0)
        {
            freq += inc * 10.f;
            freq = fclamp(freq, 20.f, 20000.f);
            osc.SetFreq(freq);
        }

        if(enc.RisingEdge())
        {
            triggerAmp = 1.0f;
            trigger = true;
        }

        // Check for incoming UART byte from ESP32
        uint8_t byte;
        if(uart.PollReceive(&byte, 1, 0) == 0)
        {
            triggerAmp = byte / 255.0f * 0.5f;
            trigger = true;
        }
    }
}