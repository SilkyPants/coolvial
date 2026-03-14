
// DisplayManager.cpp
#include "DisplayManager.h"

void DisplayManager::begin()
{
    u8g2.begin();
    printStr(0, 10, "CoolVial Booting...", u8g2_font_ncenB08_tr);
}

void DisplayManager::update(float block, float ambient, bool isCooling, const char *ipAddress)
{
    u8g2.firstPage();
    do
    {
        // 1. Average Temp Line (Top)
char avgBufferStr[32]; 

        if (ambient <= -100.0) {
            // Handle disconnected sensor gracefully
            sprintf(avgBufferStr, "Avg Temp: --");
        } else {
            sprintf(avgBufferStr, "Avg Temp: %.1f °C", ambient);
        }

        u8g2.setFont(u8g2_font_ncenB08_tf); // Choose a nice font
        u8g2.drawUTF8(0, 12, avgBufferStr);

        // 2. Main Current Temp (Middle)
        if (block > -100.0)
        {
            char str[10];
            dtostrf(block, 4, 1, str); // Using 1 decimal place to prevent overlap

            u8g2.setFont(u8g2_font_logisoso24_tf);
            u8g2.drawStr(0, 48, str);

            // Degree symbol aligned with the large text
            u8g2.setFont(u8g2_font_ncenB08_tf); // Switch back to smaller font for the symbol
            u8g2.drawUTF8(75, 48, "°C");
        }
        else
        {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 40, "Sensor Error!");
        }

        // 3. IP Address Footer (Bottom)
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 64, ipAddress);

    } while (u8g2.nextPage());
}

void DisplayManager::printStr(uint8_t x, uint8_t y, const char *msg, const uint8_t *font)
{
    u8g2.firstPage();
    do
    {
        u8g2.setFont(font); // Choose a nice font
        u8g2.drawUTF8(x, y, msg);
    } while (u8g2.nextPage());
}