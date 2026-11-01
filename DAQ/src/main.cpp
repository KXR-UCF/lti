#include <Arduino.h>
#include <ADS1256.h>
#include <SPI.h>

const int csPin = 8;
const int drdyPin = 22;
const int resetPin = 24;

ADS1256 adc(csPin, drdyPin, resetPin);

void setup() {
  Serial.begin(115200);
  delay(1000);

  adc.begin();

  adc.setGain(GAIN_1);
  adc.setDataRate(SPS_100);
  adc.selfCalibrate();
  Serial.println("Calibration done");
}

void loop() {
  //ADC Manager test code to be written
}