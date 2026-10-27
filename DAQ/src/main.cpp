#include <Arduino.h>
#include <ADS1256.h>
#include <SPI.h>

const int csPin = 8;
const int drdyPin = 22;
const int resetPin = 24;

ADS1256 adc(csPin, drdyPin, resetPin);

const uint8_t posChannels[] = {0, 1, 2, 3, 4, 5, 6, 7};
const uint8_t negChannels[] = {8, 8, 8, 8, 8, 8, 8, 8};
constexpr uint8_t NUM_CHANNELS = sizeof(posChannels) / sizeof(posChannels[0]);

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
  float values[NUM_CHANNELS];
  adc.readAllChannels(posChannels, negChannels, values, NUM_CHANNELS);

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    Serial.print("CH");
    Serial.print(i);
    Serial.print("=");
    Serial.print(values[i], 4);
    Serial.print(i < NUM_CHANNELS - 1 ? "  " : "\n");
  }

  delay(250);
}