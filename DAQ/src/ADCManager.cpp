#include "ADCManager.h"

ADCManager::ADCManager(const AdcConfig* adcs, uint8_t numAdcs) {
    if(numAdcs > MAX_ADCS) {
        _numAdcs = MAX_ADCS;
    }
    else {
        _numAdcs = numAdcs;
    }

    for (int i = 0; i < _numAdcs; i++) {
        _adcs[i] = adcs[i];  
    }
}

bool ADCManager::begin() {
    for (int i = 0; i < _numAdcs; i++) {
        if (!(_adcs[i].adc->begin())) {
            return false;
        }

        _adcs[i].adc->setDataRate(_adcs[i].dataRate);
        _adcs[i].adc->setGain(_adcs[i].gain);
        _adcs[i].adc->selfCalibrate();
    }

    return true;
}

bool ADCManager::read(uint8_t adcIndex, uint8_t channelIndex, Reading& out) {
    if(adcIndex >= _numAdcs) {
        return false;
    }
    if(channelIndex >= _adcs[adcIndex].numChannels) {
        return false;
    }

    ADS1256 *adc = _adcs[adcIndex].adc;
    const ChannelConfig& ch = _adcs[adcIndex].channels[channelIndex];

    adc->setChannel(ch.pos, ch.neg);

    out.raw = adc->readRaw();
    out.volts = adc->rawToVolts(out.raw);
    out.value = out.volts * ch.scale + ch.zero;
    out.timestampUs = micros();
    out.valid = true;

    return true;
}

uint8_t ADCManager::readAll(Reading* out, uint8_t outCapacity) {
    if(outCapacity < getNumChannels()) {
        return 0;
    }

    int n = 0;

    for(int i = 0; i < _numAdcs; i++) {
        for(int j = 0; j < _adcs[i].numChannels; j++) {
            read(i, j, out[n]);
            n++;
        }
    }

    return n;
}

uint8_t ADCManager::getNumChannels() const {
    int total = 0;

    for(int i = 0; i < _numAdcs; i++) {
        total += _adcs[i].numChannels;
    }

    return total;
}

void ADCManager::setZero(uint8_t adcIndex, uint8_t channelIndex, float zero) {
    if(adcIndex >= _numAdcs) {
        return;
    }
    if(channelIndex >= _adcs[adcIndex].numChannels) {
        return;
    }

    _adcs[adcIndex].channels[channelIndex].zero = zero;
}