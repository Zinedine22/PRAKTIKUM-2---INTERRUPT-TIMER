#include <Arduino.h>

#define BUTTON_PIN  PB0
#define LED_PIN     PC13

volatile uint32_t count = 0;
volatile bool flag = false;

void buttonISR() {
    count++;
    flag = true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Program 01: EXTI Interrupt - STM32\n");
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    Serial.println("EXTI configured on PB0 (FALLING edge)");
}

void loop() {
    if (flag) {
        flag = false;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        Serial.printf("Interrupt #%lu\n", count);
    }
}
