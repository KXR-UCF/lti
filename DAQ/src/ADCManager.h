#pragma once
#include <Arduino.h>
#include "ADS1256.h"

constexpr uint8_t MAX_ADCS             = 2;
constexpr uint8_t MAX_CHANNELS_PER_ADC = 8;
constexpr uint8_t CHANNEL_NAME_LEN     = 24;

struct ChannelConfig {
    char    name[CHANNEL_NAME_LEN];
    uint8_t pos;
    uint8_t neg;
    float   zero  = 0.0f;
    float   scale = 1.0f;
};

struct AdcConfig {
    ADS1256*      adc;
    Gain          gain;
    DataRate      dataRate;
    uint8_t       numChannels;
    ChannelConfig channels[MAX_CHANNELS_PER_ADC];
};

struct Reading {
    int32_t raw;
    float    volts;
    float    value;
    uint32_t timestampUs;
    bool     valid;
};

class ADCManager {
public:
    ADCManager(const AdcConfig* adcs, uint8_t numAdcs);

    bool begin();
    uint8_t readAll(Reading* out, uint8_t outCapacity);
    bool read(uint8_t adcIndex, uint8_t channelIndex, Reading& out);
    uint8_t getNumChannels() const;  
    void    setZero(uint8_t adcIndex, uint8_t channelIndex, float zero);

private:
    AdcConfig _adcs[MAX_ADCS];
    uint8_t   _numAdcs;
};