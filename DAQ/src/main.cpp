#include <Arduino.h>
#include <ADS1256.h>
#include <ADCManager.h>
#include <SPI.h>

ADS1256 adc1(8, 22, 24);   // cs, drdy, reset

AdcConfig adcTable[] = {
  { &adc1, GAIN_1, SPS_100, 8, {
      {"ch0", 0, 8}, 
      {"ch1", 1, 8},
      {"ch2", 2, 8},
      {"ch3", 3, 8},
      {"ch4", 4, 8},
      {"ch5", 5, 8},
      {"ch6", 6, 8},
      {"ch7", 7, 8},
  } }
};

ADCManager manager(adcTable, 1);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {} 

  if (!manager.begin()) {
    while (true) {
      Serial.println("ADC init failed");
      delay(1000);
    }
  }
  Serial.println("ADC init ok");
}

void loop() {
  Reading out[4];
  uint8_t n = manager.readAll(out, 4);

  for (uint8_t i = 0; i < n; i++) {
    Serial.print("ch");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(out[i].volts, 4);
    Serial.print("  ");
  }
  Serial.println();

  delay(200);
}