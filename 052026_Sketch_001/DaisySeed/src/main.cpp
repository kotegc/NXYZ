#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hardware;
Oscillator osc;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        float sig  = osc.Process() * 0.5f;
        out[i]     = sig;
        out[i + 1] = sig;
    }
}

int main(void)
{
    hardware.Configure();
    hardware.Init();
    hardware.SetAudioBlockSize(4);
    osc.Init(hardware.AudioSampleRate());
    osc.SetFreq(440.f);
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetAmp(1.f);
    hardware.StartAudio(AudioCallback);
    for(;;) {}
}