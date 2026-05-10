#include "display.h"

#include <Wire.h>
#include <U8g2lib.h>

namespace
{
constexpr int SDA_PIN = 5;
constexpr int SCL_PIN = 6;

U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

void drawCenteredText(int y, const char *text)
{
    int width = oled.getDisplayWidth();
    int textWidth = oled.getStrWidth(text);
    int x = (width - textWidth) / 2;
    if (x < 0)
    {
        x = 0;
    }
    oled.drawStr(x, y, text);
}
}

void initDisplay()
{
    Wire.begin(SDA_PIN, SCL_PIN);
    oled.begin();
    oled.setFont(u8g2_font_5x8_tr);
}

void renderDisplay(const String &line1, const String &line2, const String &line3, const String &line4)
{
    oled.clearBuffer();

    oled.setFont(u8g2_font_5x8_tr);
    drawCenteredText(8, line1.c_str());

    oled.setFont(u8g2_font_4x6_tr);
    drawCenteredText(18, line2.c_str());

    oled.setFont(u8g2_font_5x8_tr);
    drawCenteredText(28, line3.c_str());
    drawCenteredText(38, line4.c_str());

    oled.sendBuffer();
}