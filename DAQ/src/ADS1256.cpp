#include "ADS1256.h"

ADS1256::ADS1256(uint8_t csPin, uint8_t drdyPin, uint8_t resetPin) {
    _cs = csPin;
    _drdy = drdyPin;
    _reset = resetPin;
    _gain = 1;
    _vref = 2.5f;
    _rate = SPS_30000;
}

//Set the pins as in/out and default outputs, send reset command and check that MUX returned to default values
bool ADS1256::begin() {
    pinMode(_cs, OUTPUT);
    pinMode(_drdy,  INPUT);
    pinMode(_reset, OUTPUT);

    digitalWrite(_cs, HIGH);
    digitalWrite(_reset, HIGH);
    SPI.begin();

    reset();

    writeRegister(REG_STATUS, 0x04);

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

    //The neg and pos are both 4 bit values, make sure that the most signifcant 4 bits are all 0's
    pos &= 0b00001111;
    neg &= 0b00001111;

    //Combine the neg and pos into one value
    uint8_t select = (pos << 4) | (neg << 0);

    //Write the new channel selection to the MUX
    writeRegister(REG_MUX, select);
    sendCommand(CMD_SYNC);
    sendCommand(CMD_WAKEUP);
    settlingDelayUs();
}

void ADS1256::setGain(Gain gain) {
    uint8_t adcon = readRegister(REG_ADCON); //Read the current ADCON status
    adcon = (adcon & 0b11111000) | gain; //Set the PGA bits to 0, keep the rest the same. Than Or it with the new gain bits
    writeRegister(REG_ADCON, adcon); //Write the new ADCON back to the register

    _gain = 1 << gain; //Store the gain multiplier

    waitLowDRDY();
}

void ADS1256::setDataRate(DataRate rate) {
    writeRegister(REG_DRATE, rate);
    _rate = rate;

    waitLowDRDY();
}

int32_t ADS1256::readRaw() {
    waitLowDRDY();
    
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);

    //Reads 24 bits, broken into 3 bytes
    uint8_t d0 = 0;
    uint8_t d1 = 0;
    uint8_t d2 = 0;

    SPI.transfer(CMD_RDATA);
    delayMicroseconds(10);

    //Send dummy byte (0x00) in order to get each byte of the data
    d0 = SPI.transfer(0x00);
    d1 = SPI.transfer(0x00);
    d2 = SPI.transfer(0x00);

    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    
    //Combine all of the bytes into one value
    int32_t read = (d0 << 16) | (d1 << 8) | (d2 << 0);

    //Apply 2's complement if needed since data is signed
    if (read & 0x800000) { 
        read -= 0x1000000;      
    }

    return read;
}

float ADS1256::readVolts() {
    int32_t raw = readRaw();
    return rawToVolts(raw);
}

void ADS1256::waitLowDRDY() {
    //Store start time for timeout tracking
    int t = millis();

    //Wait for DRDY to go low
    while(digitalRead(_drdy) == HIGH) {
        //Check if time elapsed has reached timeout
        if(millis() - t > 500) {
            Serial.println("wait low hang");
            break;
        }
    };
}

void ADS1256::sendCommand(CMD cmd) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(_cs, LOW);

    //Send command, wait for timing
    SPI.transfer(cmd);
    delayMicroseconds(10);
    
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void ADS1256::settlingDelayUs() {
    //Check current data rate, and use correct settling delay
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
    //Convert the raw read into volts, incorporating gain
    float volts = (raw * 2.0f * _vref) / (_gain * 8388607.0f);
    return volts;
}

void ADS1256::readAllChannels(const uint8_t* posChannels, const uint8_t* negChannels, float* values, uint8_t numChannels) {
    
    for(int i = 0; i < numChannels; i++) {
        setChannel(posChannels[i], negChannels[i]);
        values[i] = readVolts();
    }
}