/*This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-sa/4.0/deed.en
*/
#ifdef __AVR__
// #include <avr/power.h>
#include <avr/pgmspace.h>
#endif
#include <Wire.h>
#include "vfd.h"
#include <Adafruit_NeoPixel.h>
#include <string.h>

/*    _____
     |  e  |
    c|     |f
     |_____|
     |  d  |
    b|     |g
     |_____|.p
        a     */
#define NTUBES 6
#define SEG_A (1 << 7)
#define SEG_B (1 << 6)
#define SEG_C (1 << 5)
#define SEG_D (1 << 4)
#define SEG_E (1 << 3)
#define SEG_F (1 << 2)
#define SEG_G (1 << 1)
#define SEG_DP (1 << 0)

const uint8_t letters[] PROGMEM = {
    SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G, // A
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_G,         // b
    SEG_A + SEG_B + SEG_C + SEG_E,                 // C
    SEG_A + SEG_B + SEG_D + SEG_G + SEG_F,         // d
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E,         // E
    SEG_B + SEG_C + SEG_D + SEG_E,                 // F
    SEG_A + SEG_B + SEG_C + SEG_E + SEG_G,         // G
    SEG_B + SEG_C + SEG_D + SEG_F + SEG_G,         // H
    SEG_G,                                         // I
    SEG_A + SEG_B + SEG_F + SEG_G,                 // J
    SEG_B + SEG_C + SEG_D,                         // K
    SEG_A + SEG_B + SEG_C,                         // L
    SEG_B + SEG_D + SEG_E + SEG_G,                 // M
    SEG_B + SEG_D + SEG_G,                         // N
    SEG_A + SEG_B + SEG_D + SEG_G,                 // O
    SEG_B + SEG_C + SEG_D + SEG_E + SEG_F,         // P
    SEG_C + SEG_D + SEG_E + SEG_F + SEG_G,         // Q
    SEG_B + SEG_D,                                 // R
    SEG_A + SEG_C + SEG_D + SEG_E + SEG_G,         // S
    SEG_A + SEG_B + SEG_C + SEG_D,                 // T
    SEG_A + SEG_B + SEG_C + SEG_F + SEG_G,         // U
    SEG_A + SEG_B + SEG_G,                         // V
    SEG_A + SEG_C + SEG_D + SEG_F,                 // W
    SEG_B + SEG_C + SEG_D + SEG_F + SEG_G,         // X
    SEG_A + SEG_C + SEG_D + SEG_F + SEG_G,         // Y
    SEG_A + SEG_B + SEG_D + SEG_E + SEG_F          // Z
};

const uint8_t numbers[] PROGMEM = {
    SEG_A + SEG_B + SEG_C + SEG_E + SEG_F + SEG_G,         // 0
    SEG_F + SEG_G,                                         // 1
    SEG_A + SEG_B + SEG_D + SEG_E + SEG_F,                 // 2
    SEG_A + SEG_D + SEG_E + SEG_F + SEG_G,                 // 3
    SEG_C + SEG_D + SEG_F + SEG_G,                         // 4
    SEG_A + SEG_C + SEG_D + SEG_E + SEG_G,                 // 5
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_G,         // 6
    SEG_E + SEG_F + SEG_G,                                 // 7
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G, // 8
    SEG_A + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G,         // 9
};

uint8_t i2caddr;
String vfdtxt; // Processed text to show on tubes
int txtlen;

void vfd_init(const uint8_t addr, const uint8_t rst_pin, const uint8_t clk_pin)
{
    Wire.begin();
    pinMode(rst_pin, OUTPUT);
    pinMode(clk_pin, OUTPUT);
    digitalWrite(RST, HIGH);
    digitalWrite(RST, LOW);
    i2caddr = addr;
}

int scroll_pos;
String ticker(String in, int tick_time)
{
    static uint32_t last_millis = millis();
    static uint32_t last_millis2 = millis();
    static String tick;
    bool run = false;

    if (millis() - last_millis2 > 2000) // Wait time over, start scrolling
    {
        run = true;
    }

    if (scroll_pos == 0) // New string, reset wait time
    {
        run = true;
        last_millis2 = millis();
    }

    if (millis() - last_millis > tick_time && run)
    {
        last_millis = millis();
        if (scroll_pos <= txtlen - NTUBES)
        {
            tick = "";
            int endpos = NTUBES;
            for (int i = scroll_pos; i < scroll_pos + endpos; i++)
            {
                in[i] == '.' ? endpos++ : endpos; // Copy one more
                tick += in[i];
            }
            scroll_pos++;
        }
        // Serial.println(tick + "--");
    }
    return tick;
}

uint8_t i = 0;
void vfd_multiplex()
{
    // Serial.println("-->vfd_multiplex()\n");
    static int last_millis = millis();
    if (millis() - last_millis < 1)
        return; // Execute at most every 2 milliseconds
    last_millis = millis();

    String vfdtext = vfdtxt;
    if (txtlen > NTUBES)
        vfdtext = ticker(vfdtxt, 300);

    Wire.beginTransmission(i2caddr);
    Wire.write(0); // Clear tubes
    Wire.endTransmission();
    if (vfdtext[++i] == 0) // End of string
    {
        digitalWrite(RST, HIGH);
        digitalWrite(RST, LOW);
        i = 0;
    }

    uint8_t c = vfdtext[i];
    if (c >= '0' && c <= '9')
        // c = numbers[c - '0'];
        c = pgm_read_byte_near(numbers + c - '0');
    else if (c >= 'a' && c <= 'z')
        // c = letters[c - 'a'];
        c = pgm_read_byte_near(letters + c - 'a');
    else
        switch (c)
        {
        case 32:
            c = 0;
            break;
        case '-':
            c = SEG_D;
            break;
        case '*': // Replacement for ° (Degree)
            c = SEG_C + SEG_D + SEG_E + SEG_F;
            break;
        default:
            c = 0;
        }

    if (vfdtext[i + 1] == '.') // also replacement for ':' and ','
    {
        c |= SEG_DP;
        i ? i += 1 : i;
    }
    if (i)
    {
        digitalWrite(CLK, HIGH);
        digitalWrite(CLK, LOW);
    }

    if (c)
    {
        Wire.beginTransmission(i2caddr);
        Wire.write(c);
        Wire.endTransmission();
    }
}

void vfd_set(String text)
{
    // Serial.printf("-->vfd_set(%s)\n", text.c_str());
    text.replace("°", "*");
    text.replace(":", ".");
    text.replace(",", ".");
    text.toLowerCase();
    txtlen = text.length();
    text.lastIndexOf(".") >= txtlen-NTUBES? txtlen-- : txtlen;
    //Serial.printf("text: %s, length: %d, index: %d\n", text.c_str(), txtlen, text.lastIndexOf("."));
    scroll_pos = 0;
    vfdtxt = text;
}
