#include <Arduino.h>
#include <ADS1256.h>
#include <SPI.h>

const int csPin = 8;
const int drdyPin = 22;
const int resetPin = 24;

ADS1256 adc = ADS1256(csPin, drdyPin, resetPin);

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!adc.begin()) {
    Serial.println("ADS1256 Init Failed");
  } else {
    Serial.println("ADS1256 Init Succeeded");
  }

  adc.setGain(GAIN_1);
  adc.setDataRate(SPS_100);   
  adc.setChannel(0, 8);

  adc.selfCalibrate();
  Serial.println("Calibration done");

  Serial.println("--- Starting RDATA test ---");
  adc.startContinious();
}

void loop() {
  int32_t raw = adc.readRaw();
  float volts = adc.rawToVolts(raw); 

  Serial.print("raw = ");
  Serial.print(raw);
  Serial.print("   volts = ");
  Serial.println(volts, 5);
}