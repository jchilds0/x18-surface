/*
 * channel-test.ino
 *
 * Testing a single channel wiring
 *
 */

#include <Wire.h>

#define MAX7219_DIN      32
#define MAX7219_LOAD     33
#define MAX7219_CLK      25

#define MAX7219_DECODE_MODE          0x09
#define MAX7219_INTENSITY            0x0a
#define MAX7219_SCAN_LIMIT           0x0b
#define MAX7219_SHUTDOWN             0x0c
#define MAX7219_DISPLAY_TEST         0x0f

static void max7219_init(void);
static void max7219_message(byte addr, byte data);

void setup() {
    Serial.begin(115200);
    max7219_init();
}

void loop() {
    byte digits[] = {1, 2, 3, 4};

    for (byte i = 0; i < sizeof(digits) / sizeof(digits[0]); i++) {
        for (byte seg = 2; seg < 7; seg++) {
            Serial.printf("digit %d, segment %d\n", digits[i], seg);

            max7219_message(digits[i], 1 << seg);
            delay(500);
            max7219_message(digits[i], 0);
        }
    }
}

static void max7219_init(void) {
    pinMode(MAX7219_DIN, OUTPUT);
    pinMode(MAX7219_LOAD, OUTPUT);
    pinMode(MAX7219_CLK, OUTPUT);

    delay(10);

    max7219_message(MAX7219_DISPLAY_TEST, 0x01);

    delay(1000);

    max7219_message(MAX7219_DISPLAY_TEST, 0x00);

    delay(10);

    max7219_message(MAX7219_SCAN_LIMIT, 0x07);
    max7219_message(MAX7219_DECODE_MODE, 0x00);
    max7219_message(MAX7219_INTENSITY, 0x0F);
    max7219_message(MAX7219_SHUTDOWN, 0x01);

    delay(10);

    for (int i = 1; i <= 8; i++) {
        max7219_message(i, 0x00);
    }

    Serial.println("[MAX7219] initialised");
}

static void max7219_message(byte addr, byte data) {
    digitalWrite(MAX7219_LOAD, LOW);
    digitalWrite(MAX7219_CLK, LOW);
    shiftOut(MAX7219_DIN, MAX7219_CLK, MSBFIRST, addr);
    shiftOut(MAX7219_DIN, MAX7219_CLK, MSBFIRST, data);
    digitalWrite(MAX7219_LOAD, HIGH);
}

