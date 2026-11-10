#include "backlight.h"
#include <Wire.h>
#include <Arduino.h>
#include "debug_log.h"

void sendI2CCommand(uint8_t command) {
    // Start sending commands to the specified address
    uint8_t err = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        Wire.beginTransmission(0x30);
        // Send command
        Wire.write(command);
        //  End transmission and return status
        err = Wire.endTransmission();
        if (err == 0)
            break;
    }
    if (err == 0) {
        debug_printf("command 0x%02X sent\n", command);
    } else {
        Serial.print("Command sent error, error code:");
        Serial.println(err);
    }
}
static void set_backlight(int32_t x) {
    if (x > DIMMEST) {
        x = DIMMEST;
    } else if (x < 0) {
        x = 0;
    }
    sendI2CCommand(x);
}

void backlight_set_percent(int32_t percent) {
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    int32_t x = DIMMEST - percent * percent * DIMMEST / 10000;
    set_backlight(x);
}
