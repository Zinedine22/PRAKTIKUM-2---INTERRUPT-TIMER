#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Tombol menggunakan D33
#define BUTTON_PIN      33
#define START_STOP_PIN  32
// LED internal ESP32 tetap di D2
#define LED_PIN         2
#define STOP_LED_PIN    14
#define RUN_LED_PIN     12
#define OLED_SDA_PIN    21
#define OLED_SCL_PIN    22
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET_PIN  -1
#define OLED_ADDR       0x3C
#define DEBOUNCE_US     50000   // 50ms in microseconds
#define START_DEBOUNCE_MS 200UL
#define ACTIVE_MS       10000UL // 10 detik
#define IDLE_MS         15000UL // 15 detik

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);
bool oledReady = false;
uint8_t oledAddress = OLED_ADDR;
uint32_t lastOledRefreshMs = 0;
uint32_t lastHeartbeatMs = 0;

volatile uint32_t lastInterrupt = 0;
volatile uint32_t pressCount = 0;
volatile bool newPress = false;

enum Mode {
    MODE_ACTIVE,
    MODE_IDLE
};

Mode currentMode = MODE_ACTIVE;
uint32_t modeStartMs = 0;
bool isRunning = false;
bool lastStartButtonState = HIGH;
uint32_t lastStartDebounceMs = 0;

const char* modeText(Mode mode) {
    return mode == MODE_ACTIVE ? "ACTIVE" : "IDLE";
}

uint32_t modeDurationMs(Mode mode) {
    return mode == MODE_ACTIVE ? ACTIVE_MS : IDLE_MS;
}

void setProgramState(bool running, uint32_t nowMs) {
    isRunning = running;
    digitalWrite(STOP_LED_PIN, running ? LOW : HIGH);
    digitalWrite(RUN_LED_PIN, running ? HIGH : LOW);

    if (!running) {
        digitalWrite(LED_PIN, LOW);
    } else {
        currentMode = MODE_ACTIVE;
        modeStartMs = nowMs;
        noInterrupts();
        newPress = false;
        interrupts();
    }
}

bool findOledAddress(uint8_t &foundAddress) {
    const uint8_t candidates[] = {0x3C, 0x3D};
    for (uint8_t i = 0; i < 2; i++) {
        Wire.beginTransmission(candidates[i]);
        if (Wire.endTransmission() == 0) {
            foundAddress = candidates[i];
            return true;
        }
    }
    return false;
}

void scanI2CBus() {
    Serial.println("[I2C] scanning...");
    uint8_t foundCount = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("[I2C] found device at 0x%02X\n", addr);
            foundCount++;
        }
    }

    if (foundCount == 0) {
        Serial.println("[I2C] no device found");
    }
}

void updateDisplay(bool force = false) {
    if (!oledReady) {
        return;
    }

    uint32_t nowMs = millis();
    if (!force && (nowMs - lastOledRefreshMs < 500)) {
        return;
    }
    lastOledRefreshMs = nowMs;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Program: ");
    display.println(isRunning ? "RUNNING" : "STOP");
    display.setCursor(0, 14);
    display.print("Mode   : ");
    display.println(isRunning ? modeText(currentMode) : "-");

    display.setCursor(0, 28);
    if (!isRunning) {
        display.println("Press D32 to START");
        display.setCursor(0, 42);
        display.println("D14=STOP D12=RUN");
        display.display();
        return;
    }

    display.setTextSize(2);
    display.setCursor(0, 42);
    uint32_t elapsedMs = nowMs - modeStartMs;
    uint32_t durationMs = modeDurationMs(currentMode);
    uint32_t remainingMs = elapsedMs >= durationMs ? 0 : (durationMs - elapsedMs);
    uint32_t remainingSec = (remainingMs + 999) / 1000;
    display.print(remainingSec);
    display.print("s");
    display.display();
}

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

    Serial.println("Program 02: ACTIVE/IDLE LED Control - ESP32\n");

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(START_STOP_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(STOP_LED_PIN, OUTPUT);
    pinMode(RUN_LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    setProgramState(false, millis());

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    modeStartMs = millis();

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeOut(50);
    scanI2CBus();
    oledReady = findOledAddress(oledAddress);
    if (oledReady) {
        oledReady = display.begin(SSD1306_SWITCHCAPVCC, oledAddress);
    }

    if (!oledReady) {
        Serial.println("OLED tidak terdeteksi (cek wiring SDA/SCL/VCC/GND)");
    } else {
        Serial.printf("OLED terdeteksi di alamat 0x%02X\n", oledAddress);
        updateDisplay(true);
    }

    Serial.printf("Debounce time: %d us\n", DEBOUNCE_US);
    Serial.println("D32 = START/STOP program");
    Serial.println("D14 = STOP indicator, D12 = RUNNING indicator");
    Serial.println("Mode ACTIVE (10 detik): PB toggle LED");
    Serial.println("Mode IDLE  (15 detik): LED ON terus, PB diabaikan\n");
}

void loop() {
    uint32_t nowMs = millis();

    bool startButtonState = digitalRead(START_STOP_PIN);
    if (lastStartButtonState == HIGH && startButtonState == LOW &&
        (nowMs - lastStartDebounceMs >= START_DEBOUNCE_MS)) {
        lastStartDebounceMs = nowMs;
        setProgramState(!isRunning, nowMs);
        Serial.printf("[PROGRAM] %s\n", isRunning ? "RUNNING" : "STOP");
        updateDisplay(true);
    }
    lastStartButtonState = startButtonState;

    if (!isRunning) {
        noInterrupts();
        newPress = false;
        interrupts();
        updateDisplay();
        return;
    }

    if (currentMode == MODE_ACTIVE && (nowMs - modeStartMs >= ACTIVE_MS)) {
        currentMode = MODE_IDLE;
        modeStartMs = nowMs;
        digitalWrite(LED_PIN, HIGH);

        noInterrupts();
        newPress = false;
        interrupts();

        Serial.println("[MODE] IDLE: LED ON, push button diabaikan");
        updateDisplay(true);
    } else if (currentMode == MODE_IDLE && (nowMs - modeStartMs >= IDLE_MS)) {
        currentMode = MODE_ACTIVE;
        modeStartMs = nowMs;
        Serial.println("[MODE] ACTIVE: push button mengontrol LED");
        updateDisplay(true);
    }

    if (currentMode == MODE_ACTIVE) {
        bool pressed = false;
        uint32_t capturedCount = 0;
        uint32_t capturedTime = 0;

        noInterrupts();
        if (newPress) {
            newPress = false;
            pressed = true;
            capturedCount = pressCount;
            capturedTime = lastInterrupt;
        }
        interrupts();

        if (pressed) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            Serial.printf("Press #%lu at %lu us\n", capturedCount, capturedTime);
        }
    }

    updateDisplay();

    if (nowMs - lastHeartbeatMs >= 2000) {
        Serial.printf("[HEARTBEAT] mode=%s led=%d\n", modeText(currentMode), digitalRead(LED_PIN));
        lastHeartbeatMs = nowMs;
    }
}