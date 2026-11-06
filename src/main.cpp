#include "LovyanGFX_Driver.h"
#include "pins_config.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <lvgl.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdbool.h>

#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "clock.h"
#include "rtc.h"
#include "screen_main.h"
#include <sys/time.h>

#include <ArduinoJson.h>

LGFX gfx;

uint16_t touch_x, touch_y;
static esp_lcd_panel_handle_t rgb_panel = nullptr;
static SemaphoreHandle_t vsync_sem = nullptr;

static bool IRAM_ATTR on_rgb_vsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data,
                                   void *user_ctx) {
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(vsync_sem, &task_woken);
    return task_woken == pdTRUE;
}

//  Display refresh
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    // In DIRECT mode LVGL can report several dirty areas for one frame.  The
    // framebuffer must be presented only after the last area is complete.
    if (lv_display_flush_is_last(disp)) {
        // Discard an old VSYNC notification, request the framebuffer switch, then
        // wait until the LCD controller reaches the next vertical blanking edge.
        xSemaphoreTake(vsync_sem, 0);
        esp_err_t err = esp_lcd_panel_draw_bitmap(rgb_panel, 0, 0, LCD_H_RES, LCD_V_RES, px_map);
        if (err == ESP_OK) {
            xSemaphoreTake(vsync_sem, pdMS_TO_TICKS(100));
        }
    }
    lv_display_flush_ready(disp);
}

//  Read touch
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    data->state = LV_INDEV_STATE_REL; // The state of data existence when releasing the finger
    lgfx::touch_point_t point;
    if (gfx._touch_instance.getTouchRaw(&point, 1)) {
        data->state = LV_INDEV_STATE_PR;

        //  Set coordinates
        data->point.x = point.x;
        data->point.y = point.y;
    }
}

uint32_t my_tick_get_cb() { return millis(); }

bool i2cScanForAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

// Wrapper function for sending I2C commands
extern "C" void sendI2CCommand(uint8_t command) {
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
        Serial.print("command 0x");
        Serial.print(command, HEX);
        Serial.println(" Sent successfully");
    } else {
        Serial.print("Command sent error, error code:");
        Serial.println(err);
    }
}

static void handle_line(const char *line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        Serial.printf("bad line: %s\n", err.c_str());
        return;
    }
    if (doc["epoch"].is<long long>()) {
        long long epoch = doc["epoch"].as<long long>();
        const char *tz = doc["tz"] | "";
        clock_sync(epoch, tz);
        Serial.printf("clock_sync(%lld, \"%s\")\n", epoch, tz);
    } else {
        Serial.println("bad line: no epoch");
    }
}

static void serial_poll(void) {

    static char line[512];
    static size_t len = 0;

    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            line[len] = 0;
            if (len > 0) {
                Serial.printf("Serial command: %s\n", line);
                len = 0;
                handle_line(line);
            }
        } else if (len < sizeof(line) - 1) {
            if (c >= 0x20 && c < 0x7F) // Only accept printable ASCII characters
            {
                line[len++] = c;
            }
        }
    }
}

void setup() {

    Serial.setRxBufferSize(1024);
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
    // Start sending command 0 to address 0x30
    sendI2CCommand(0); // 0 is the brightest backlight.    / 245 backlight off   (0-245)

    time_t now;
    if (rtc_read(&now)) {
        Serial.printf("RTC: %lld\n", (long long)now);
        struct timeval tv = {};
        tv.tv_sec = now;
        settimeofday(&tv, nullptr);
    } else {
        Serial.println("RTC: not set");
    }
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Init touch independently. The RGB display is driven by ESP-IDF below so
    // that two real scan-out framebuffers can be used.
    if (!gfx._touch_instance.init()) {
        Serial.println("Failed to initialize GT911 touch controller");
    }
    i2cScanForAddress(0x30);

    vsync_sem = xSemaphoreCreateBinary();
    if (vsync_sem == nullptr) {
        Serial.println("Failed to create RGB VSYNC semaphore");
        abort();
    }

    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.timings.pclk_hz = 18000000;
    panel_config.timings.h_res = LCD_H_RES;
    panel_config.timings.v_res = LCD_V_RES;
    panel_config.timings.hsync_pulse_width = 4;
    panel_config.timings.hsync_back_porch = 8;
    panel_config.timings.hsync_front_porch = 8;
    panel_config.timings.vsync_pulse_width = 4;
    panel_config.timings.vsync_back_porch = 8;
    panel_config.timings.vsync_front_porch = 8;
    panel_config.timings.flags.hsync_idle_low = 0;
    panel_config.timings.flags.vsync_idle_low = 0;
    panel_config.timings.flags.pclk_active_neg = 1;
    panel_config.timings.flags.pclk_idle_high = 1;
    panel_config.data_width = 16;
    panel_config.bits_per_pixel = 16;
    panel_config.num_fbs = 2;
    // Keep the active scan data in internal SRAM while the RGB DMA is reading
    // PSRAM framebuffers. Without bounce lines, a LVGL redraw can contend with
    // scan-out and produce the horizontal block/line corruption seen on screen.
    panel_config.bounce_buffer_size_px = LCD_H_RES * 20;
    panel_config.dma_burst_size = 64;
    panel_config.hsync_gpio_num = GPIO_NUM_40;
    panel_config.vsync_gpio_num = GPIO_NUM_41;
    panel_config.de_gpio_num = GPIO_NUM_42;
    panel_config.pclk_gpio_num = GPIO_NUM_39;
    panel_config.disp_gpio_num = -1;
    const int rgb_pins[16] = {21, 47, 48, 45, 38, 9, 10, 11, 12, 13, 14, 7, 17, 18, 3, 46};
    for (int i = 0; i < 16; ++i) {
        panel_config.data_gpio_nums[i] = rgb_pins[i];
    }
    panel_config.flags.fb_in_psram = 1;
    panel_config.flags.bb_invalidate_cache = 1;

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &rgb_panel));
    esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
    callbacks.on_vsync = on_rgb_vsync;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(rgb_panel, &callbacks, nullptr));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(rgb_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(rgb_panel));

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    constexpr size_t frame_buffer_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    void *frame_buffer_1 = nullptr;
    void *frame_buffer_2 = nullptr;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(rgb_panel, 2, &frame_buffer_1, &frame_buffer_2));
    memset(frame_buffer_1, 0, frame_buffer_size);
    memset(frame_buffer_2, 0, frame_buffer_size);

    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_buffers(display, frame_buffer_1, frame_buffer_2, frame_buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    delay(100);

    screen_main_create();
    clock_start();

    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    serial_poll();
    delay(1);
}
