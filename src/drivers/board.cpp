#include "board.h"

#include <Wire.h>
#include <Arduino.h>
#include <driver/uart.h>
#include <esp_sleep.h>
#include "drivers/backlight.h"

bool board_nap(uint32_t ms) {
    Serial.flush(); // what's being sent goes out first: the UART stops while asleep
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000);
    uart_set_wakeup_threshold(UART_NUM_0, 3); // 3 edges on RX: the first byte of a message
    esp_sleep_enable_uart_wakeup(UART_NUM_0);
    esp_light_sleep_start();
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UART;
}


static bool i2cScanForAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

void board_init() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);

    Wire.begin(15, 16);
    delay(50);
    while (1) {
        if (i2cScanForAddress(0x51)) {
            Serial.print("The microcontroller has RTC: address 0x");
            Serial.println(0x51, HEX);
        }
        if (i2cScanForAddress(0x30) && i2cScanForAddress(0x5D)) {
            Serial.print("The microcontroller is detected: address 0x");
            Serial.println(0x30, HEX);
            Serial.print("The microcontroller is detected: address 0x");
            Serial.println(0x5D, HEX);
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

    backlight_set_percent(100); //full brightness
}