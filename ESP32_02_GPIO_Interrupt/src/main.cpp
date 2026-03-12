/**
 * @file main.cpp
 * @brief Program 02: GPIO Interrupt + Timestamp Debounce - ESP32
 */

#include <Arduino.h>

#define BUTTON_PIN      0
#define LED_PIN         2
#define DEBOUNCE_US     50000   // 50ms in microseconds

volatile uint32_t lastInterrupt = 0;
volatile uint32_t pressCount = 0;
volatile bool newPress = false;

void IRAM_ATTR buttonISR() {
    uint32_t now = micros();
    if (now - lastInterrupt > DEBOUNCE_US) {
        lastInterrupt = now;
        pressCount++;
        newPress = true;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Program 02: Timestamp Debounce - ESP32\n");
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    Serial.printf("Debounce time: %d us\n\n", DEBOUNCE_US);
}

void loop() {
    if (newPress) {
        newPress = false;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        Serial.printf("Press #%lu at %lu us\n", pressCount, lastInterrupt);
    }
}
