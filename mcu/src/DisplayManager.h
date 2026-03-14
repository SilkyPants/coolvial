// DisplayManager.h
#pragma once
#include <U8g2lib.h>

class DisplayManager
{
private:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2;

public:
    DisplayManager(const u8g2_cb_t *rotation, uint8_t reset = U8X8_PIN_NONE, uint8_t clock = U8X8_PIN_NONE, uint8_t data = U8X8_PIN_NONE) : u8g2(rotation, /* reset=*/reset, /* clock=*/clock, /* data=*/data) {}
    void begin();
    void update(float block, float ambient, bool isCooling, const char *ipAddress);
    void printStr(uint8_t x, uint8_t y, const char *msg, const uint8_t *font = u8g2_font_ncenB08_tf);
};