#include "board.h"

#include <Wire.h>
#include <Arduino.h>
#include "drivers/backlight.h"

// How often (about 0.25 s each) to look for the backlight controller (0x30) and the touch chip (0x5D) at
// power-up. Another board revision may not have them there: it starts anyway after this.
#define DETECT_TRIES 40


static bool i2cScanForAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

void board_init() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);

    Wire.begin(15, 16);
    delay(50);
    bool found = false;
    for (int attempt = 0; attempt < DETECT_TRIES; ++attempt) {
        if (i2cScanForAddress(0x51)) {
            Serial.print("The microcontroller has RTC: address 0x");
            Serial.println(0x51, HEX);
        }
        if (i2cScanForAddress(0x30) && i2cScanForAddress(0x5D)) {
            Serial.print("The microcontroller is detected: address 0x");
            Serial.println(0x30, HEX);
            Serial.print("The microcontroller is detected: address 0x");
            Serial.println(0x5D, HEX);
            found = true;
            break;
        } else {
            Serial.print("No microcontroller was detected: address 0x");
            Serial.println(0x30, HEX);
            Serial.print("No microcontroller was detected: address 0x");
            Serial.println(0x5D, HEX);
            // Prevent the microcontroller did not start to adjust the bright screen
            sendI2CCommand(250); // 250 : Activate touch screen
            pinMode(1, OUTPUT);
            digitalWrite(1, LOW);

            delay(120);
            pinMode(1, INPUT);

            delay(100);
        }
    }
    if (!found)
        Serial.println("Gave up looking for the backlight controller (0x30) and the touch chip (0x5D): "
                       "another board revision? The screen may stay dark or ignore touch.");

    backlight_set_percent(100); //full brightness
}