#include "ADS1256.h"

ADS1256::ADS1256(uint8_t csPin, uint8_t drdyPin, uint8_t resetPin) {
    _cs = csPin;
    _drdy = drdyPin;
    _reset = resetPin;
    _gain = 1;
    _vref = 2.5f;
    _rate = SPS_30000;
}

bool ADS1256::begin() {
    pinMode(_cs, OUTPUT);
    pinMode(_drdy,  INPUT);
    pinMode(_reset, OUTPUT);

    digitalWrite(_cs, HIGH);
    digitalWrite(_reset, HIGH);
    SPI.begin();

    reset();
    uint8_t mux = readRegister(REG_MUX);

    return (mux == 0x01);
}

void ADS1256::reset() {
    sendCommand(CMD_RESET);
    delayMicroseconds(5000);
    waitLowDRDY();
}

void ADS1256::selfCalibrate() {
    sendCommand(CMD_SELFCAL);
    waitLowDRDY();
}

uint8_t ADS1256::readRegister(Reg reg) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);

    SPI.transfer(CMD_RREG | reg); //Read register reg
    SPI.transfer(0x00); //Read one register (number of registers - 1)
    delayMicroseconds(10);
    uint8_t value = SPI.transfer(0x00); //Send dummy bit and get status

    digitalWrite(_cs, HIGH);
    SPI.endTransaction();

    return value;
}

void ADS1256::writeRegister(Reg reg, uint8_t value) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);

    SPI.transfer(CMD_WREG | reg); //Write to register reg
    SPI.transfer(0x00); //Write one register (number of registers - 1)
    SPI.transfer(value); //Send the value to write with

    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void ADS1256::setChannel(uint8_t pos, uint8_t neg) {

    pos &= 0b00001111;
    neg &= 0b00001111;
    uint8_t select = (pos << 4) | (neg << 0);

    writeRegister(REG_MUX, select);
    sendCommand(CMD_SYNC);
    sendCommand(CMD_WAKEUP);
    settlingDelay();
}

void ADS1256::setGain(Gain gain) {
    uint8_t adcon = readRegister(REG_ADCON); //Read the current ADCON status
    adcon = (adcon & 0b11111000) | gain; //Set the PGA bits to 0, keep the rest the same. Than Or it with the new gain bits
    writeRegister(REG_ADCON, adcon); //Write the new ADCON back to the register

    _gain = 1 << gain; //Store the gain multiplier
}

void ADS1256::setDataRate(DataRate rate) {
    writeRegister(REG_DRATE, rate);
    _rate = rate;
}

int32_t ADS1256::readRaw() {
    waitLowDRDY();
    
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);

    uint8_t d0 = 0;
    uint8_t d1 = 0;
    uint8_t d2 = 0;

    SPI.transfer(CMD_RDATA);
    delayMicroseconds(10);

    d0 = SPI.transfer(0x00);
    d1 = SPI.transfer(0x00);
    d2 = SPI.transfer(0x00);

    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    
    int32_t read = (d0 << 16) | (d1 << 8) | (d2 << 0);

    if (read & 0x800000) { 
        read -= 0x1000000;      
    }

    return read;
}

float ADS1256::readVolts() {
    int32_t raw = readRaw();
    return rawToVolts(raw);
}

void ADS1256::startContinious() {
    waitLowDRDY();
    
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);
    SPI.transfer(CMD_RDATAC);
    delayMicroseconds(10);
}

int32_t ADS1256::readContiniousRaw() {
    waitLowDRDY();

    uint8_t d0 = 0;
    uint8_t d1 = 0;
    uint8_t d2 = 0;

    d0 = SPI.transfer(0x00);
    d1 = SPI.transfer(0x00);
    d2 = SPI.transfer(0x00);
    
    int32_t read = (d0 << 16) | (d1 << 8) | (d2 << 0);

    if (read & 0x800000) { 
        read -= 0x1000000;      
    }

    //waitHighDRDY();

    return read;
}

float ADS1256::readContiniousVolts() {
    int32_t raw = readContiniousRaw();
    return rawToVolts(raw);
}

void ADS1256::stopContinious() {
    waitLowDRDY();
    SPI.transfer(CMD_SDATAC);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void ADS1256::waitHighDRDY() {
    int t = millis();
    while(digitalRead(_drdy) == LOW) {
        if(millis() - t > 500) {
            Serial.println("wait high hang");
            break;
        }
    };
}

void ADS1256::waitLowDRDY() {
    int t = millis();
    while(digitalRead(_drdy) == HIGH) {
        if(millis() - t > 500) {
            Serial.println("wait low hang");
            break;
        }
    };
}

void ADS1256::sendCommand(CMD cmd) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);
    SPI.transfer(cmd);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void ADS1256::settlingDelay() {
    int delay;
        switch(_rate) {
        case SPS_30000: delay = 210;    break;
        case SPS_15000: delay = 250;    break;
        case SPS_7500:  delay = 310;    break;
        case SPS_3750:  delay = 440;    break;
        case SPS_2000:  delay = 680;    break;
        case SPS_1000:  delay = 1180;   break;
        case SPS_500:   delay = 2180;   break;
        case SPS_100:   delay = 10180;  break;
        case SPS_60:    delay = 16840;  break;
        case SPS_50:    delay = 20180;  break;
        case SPS_30:    delay = 33510;  break;
        case SPS_25:    delay = 40180;  break;
        case SPS_15:    delay = 66840;  break;
        case SPS_10:    delay = 100180; break;
        case SPS_5:     delay = 200180; break;
        case SPS_2d5:   delay = 400180; break;
        default:        delay = 400180; break;
}

    delayMicroseconds(delay);
}

float ADS1256::rawToVolts(int32_t raw) {
    float volts = (raw * 2.0f * _vref) / (_gain * 8388607.0f);
    return volts;
}