#pragma once
#include <Arduino.h>
#include <SPI.h>

constexpr uint32_t SPI_CLOCK_HZ = 500000;

enum Gain : uint8_t {
  GAIN_1=0,
  GAIN_2=1,
  GAIN_4=2,
  GAIN_8=3,
  GAIN_16=4,
  GAIN_32=5,
  GAIN_64=6
};

enum DataRate : uint8_t {
  SPS_30000 = 0b11110000,
  SPS_15000 = 0b11100000,
  SPS_7500 = 0b11010000,
  SPS_3750 = 0b11000000,
  SPS_2000 = 0b10110000,
  SPS_1000 = 0b10100001,
  SPS_500 = 0b10010010,
  SPS_100 = 0b10000010,
  SPS_60 = 0b01110010,
  SPS_50 = 0b01100011,
  SPS_30 = 0b01010011,
  SPS_25 = 0b01000011,
  SPS_15 = 0b00110011,
  SPS_10 = 0b00100011,
  SPS_5 = 0b00010011,
  SPS_2d5 = 0b00000011
};

enum Reg : uint8_t {
  REG_STATUS = 0,
  REG_MUX = 1,
  REG_ADCON = 2,
  REG_DRATE = 3,
  REG_IO = 4,
  REG_OFC0 = 5,
  REG_OFC1 = 6,
  REG_OFC2 = 7,
  REG_FSC0 = 8,
  REG_FSC1 = 9,
  REG_FSC2 = 10
};

enum CMD : uint8_t {
  CMD_WAKEUP = 0x00, // Completes SYNC and Exits Standby Mode
  CMD_RDATA = 0x01, //Read Data
  CMD_RDATAC = 0x03, //Read Data Continuously
  CMD_SDATAC = 0x0F, //Stop Read Data Continuously
  CMD_RREG = 0x10, //Read from REG rrr
  CMD_WREG = 0x50, //Write to REG rrr
  CMD_SELFCAL = 0xF0, //Offset and Gain Self-Calibration
  CMD_SELFOCAL = 0xF1, //Offset Self-Calibration
  CMD_SELFGCAL = 0xF2, //Gain Self-Calibration
  CMD_SYSOCAL = 0xF3, //System Offset Calibration
  CMD_SYSGCAL = 0xF4, //System Gain Calibration
  CMD_SYNC = 0xFC, //Synchronize the A/D Conversion
  CMD_STANDBY = 0xFD, //Begin Standby Mode
  CMD_RESET = 0xFE //Reset to Power-Up Values
};

class ADS1256 {
public:
    ADS1256(uint8_t csPin, uint8_t drdyPin, uint8_t resetPin = 255);

    bool begin();            
    void reset();
    void selfCalibrate();

    uint8_t readRegister(Reg reg);
    void    writeRegister(Reg reg, uint8_t value);

    void setChannel(uint8_t pos, uint8_t neg);
    void setGain(Gain gain);
    void setDataRate(DataRate rate);

    int32_t readRaw();                 
    float   readVolts();               
    void    startContinious();
    int32_t readContiniousRaw();
    float   readContiniousVolts();
    void    stopContinious();
    float   rawToVolts(int32_t raw);

private:
    void    waitHighDRDY();
    void    waitLowDRDY();
    void    sendCommand(CMD cmd);
    void    settlingDelay();

    uint8_t _cs, _drdy, _reset;
    uint8_t _gain;
    float   _vref;
    DataRate _rate;
};