#include "daisy_seed.h"

using namespace daisy;

DaisySeed hw;

// Audio callback - this is where DSP happens
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        // Passthrough L and R for now
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // samples per block

    // UART for ESP32-S3 communication
    // Daisy Seed pins: TX=GPIO14, RX=GPIO15
    UartHandler uart;
    UartHandler::Config uart_config;
    uart_config.periph        = UartHandler::Config::Peripheral::USART_1;
    uart_config.baudrate      = 115200;
    uart_config.stopbits      = UartHandler::Config::StopBits::BITS_1;
    uart_config.parity        = UartHandler::Config::Parity::NONE;
    uart_config.mode          = UartHandler::Config::Mode::TX_RX;
    uart_config.pin_config.tx = seed::D13;
    uart_config.pin_config.rx = seed::D14;
    uart.Init(uart_config);

    hw.StartAudio(AudioCallback);

    while(1)
    {
        // Main loop - handle UART comms with ESP32-S3
        // e.g. receive parameter updates, send status back
    }
}