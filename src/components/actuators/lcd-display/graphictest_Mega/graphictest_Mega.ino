/*****************************************************************
 *  MCUFRIEND 8-bit TFT Demo – ปรับแมปขาให้ตรงกับ Arduino Mega 2560
 *****************************************************************/

#include <SPI.h>            // จำเป็นสำหรับไลบรารีบางตัว
#include "Adafruit_GFX.h"   // ไลบรารีกราฟิกพื้นฐาน
#include <MCUFRIEND_kbv.h>  // ไลบรารีสำหรับจอ mcufriend
MCUFRIEND_kbv tft;

// ─── กำหนดขา Control ของจอ (ต่อกับ Sensor-Shield บน Mega) ────
//   LCD_CS  → ขา 30 (D30 บน Mega2560)
//   LCD_CD  → ขา 31 (D31 บน Mega2560)  // (RS หรือ D/C บน Shield)
//   LCD_WR  → ขา 32 (D32 บน Mega2560)
//   LCD_RD  → ขา 33 (D33 บน Mega2560)
//   LCD_RESET → ขา 36 (D36 บน Mega2560)  // สายรีเซ็ตจอ
#define LCD_CS    30   // Chip Select ของจอ (เอาไปกำหนดใน library)
#define LCD_CD    31   // Command/Data (RS หรือ D/C)
#define LCD_WR    32   // Write Strobe
#define LCD_RD    33   // Read  Strobe
#define LCD_RESET 36   // รีเซ็ตจอ (ต่อกับขา 36 ตามที่ปรับใหม่)

// ─── ขา Data Bus 8-bit (D0–D7) ─────────────────────────────────
//   LCD_D0 → ขา 22 (D22 บน Mega2560)  
//   LCD_D1 → ขา 23 (D23 บน Mega2560)
//   LCD_D2 → ขา 24 (D24 บน Mega2560)
//   LCD_D3 → ขา 25 (D25 บน Mega2560)
//   LCD_D4 → ขา 26 (D26 บน Mega2560)
//   LCD_D5 → ขา 27 (D27 บน Mega2560)
//   LCD_D6 → ขา 28 (D28 บน Mega2560)
//   LCD_D7 → ขา 29 (D29 บน Mega2560)
//  ไลบรารี MCUFRIEND_kbv จะอ่านค่าจาก PORTA (22–29) อัตโนมัติ
//  (ไม่ต้องเขียน pinMode ให้ data-bus เพราะ library จัดการให้)

// ─── ตัวแปรและค่าพื้นฐาน ────────────────────────────────────────
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

// ─── ประกาศฟังก์ชันทดสอบกราฟิก ─────────────────────────────────
void runtests(void);
unsigned long testFillScreen();
unsigned long testText();
unsigned long testLines(uint16_t color);
unsigned long testFastLines(uint16_t color1, uint16_t color2);
unsigned long testRects(uint16_t color);
unsigned long testFilledRects(uint16_t color1, uint16_t color2);
unsigned long testFilledCircles(uint8_t radius, uint16_t color);
unsigned long testCircles(uint8_t radius, uint16_t color);
unsigned long testTriangles();
unsigned long testFilledTriangles();
unsigned long testRoundRects();
unsigned long testFilledRoundRects();

// ถ้ามีฟอนต์ภาษาจีนฝังในโปรแกรม (ไม่จำเป็นต้องแก้)
extern const uint8_t hanzi[];
void showhanzi(unsigned int x, unsigned int y, unsigned char index)
{
    uint8_t i, j, c, first = 1;
    uint8_t *temp = (uint8_t*)hanzi;
    uint16_t color;
    tft.setAddrWindow(x, y, x + 31, y + 31);
    temp += index * 128;
    for (j = 0; j < 128; j++)
    {
        c = pgm_read_byte(temp++);
        for (i = 0; i < 8; i++)
        {
            color = (c & (1 << i)) ? RED : BLACK;
            tft.pushColors(&color, 1, first);
            first = 0;
        }
    }
}

// ─── SETUP ───────────────────────────────────────────────────────
void setup(void) {
    Serial.begin(9600);

    // *** สำคัญสำหรับ Mega2560:  ต้องกำหนดขา 53 เป็น OUTPUT ***
    //    เพื่อให้บัส SPI ทำงานในโหมด Master เสมอ แม้ไม่ใช้ SD Card
    pinMode(53, OUTPUT);
    digitalWrite(53, HIGH);

    // กรณีจอมี microSD ร่วมอยู่ ให้กำหนด CS ของ SD เป็น INPUT_PULLUP/KEEP HIGH
    // (ไลบรารีไม่ได้ใช้ SD ในตัวอย่างนี้ แต่เผื่อ Shield มีช่อง SD)
    // pinMode(10, OUTPUT);
    // digitalWrite(10, HIGH);

    // เริ่มต้นไลบรารีจอ
    tft.begin(tft.readID()); 

    // ทดสอบแสดงสีพื้นสั้น ๆ
    tft.fillScreen(RED);
    delay(300);
    tft.fillScreen(GREEN);
    delay(300);
    tft.fillScreen(BLUE);
    delay(300);
    tft.fillScreen(BLACK);
    
    // รันชุดทดสอบกราฟิกทั้งหมด
    runtests();
}

// ─── LOOP ────────────────────────────────────────────────────────
void loop(void) {
    uint8_t aspect;
    uint16_t pixel;
    const char *aspectname[] = {
        "PORTRAIT", "LANDSCAPE", "PORTRAIT_REV", "LANDSCAPE_REV"
    };
    const char *colorname[] = { "BLUE", "GREEN", "RED", "GRAY" };
    uint16_t colormask[] = { 0x001F, 0x07E0, 0xF800, 0xFFFF };
    uint16_t dx, rgb, n, wid, ht, msglin;
    
    // เริ่มต้นที่การหมุน 0 (แนวตั้ง)
    tft.setRotation(0);
    runtests();
    delay(2000);

    // ถ้าความสูงจอ > 64 พิกเซล ให้วนหมุนหน้าจอทดสอบ 4 ทิศทาง
    if (tft.height() > 64) {
        for (uint8_t cnt = 0; cnt < 4; cnt++) {
            aspect = cnt & 3;
            tft.setRotation(aspect);
            wid = tft.width();
            ht  = tft.height();
            msglin = (ht > 160) ? 200 : 112;
            
            testText();
            dx = wid / 32;
            for (n = 0; n < 32; n++) {
                rgb = n * 8;
                rgb = tft.color565(rgb, rgb, rgb);
                tft.fillRect(n * dx, 48, dx, 63, rgb & colormask[aspect]);
            }
            tft.drawRect(0, 48 + 63, wid, 1, WHITE);
            tft.setTextSize(2);
            tft.setTextColor(colormask[aspect], BLACK);
            tft.setCursor(0, 72);
            tft.print(colorname[aspect]);
            tft.setTextColor(WHITE);
            tft.println(" COLOR GRADES");
            tft.setTextColor(WHITE, BLACK);
            tft.setCursor(0, msglin);
            tft.println(aspectname[aspect]);
            delay(1000);

            // อ่านพิกเซลตัวอย่างมาทดสอบ readPixel()
            tft.drawPixel(0, 0, YELLOW);
            pixel = tft.readPixel(0, 0);
            tft.setTextSize((ht > 160) ? 2 : 1);
            
            if ((pixel & 0xF8F8) == 0xF8F8) {
                tft.println("readPixel() SHOULD BE 24-bit");
            } else {
                tft.print("readPixel() reads 0x");
                tft.println(pixel, HEX);
            }
            delay(5000);
        }
    }

    // ทดสอบ invert display
    tft.fillScreen(BLACK);
    tft.setTextColor(YELLOW, BLACK);
    tft.setCursor(0, 0);
    tft.println("INVERT DISPLAY ");
    tft.invertDisplay(true);
    delay(2000);
    tft.invertDisplay(false);
    delay(2000);
}

// ─── ฟังก์ชันทดสอบกราฟิก ────────────────────────────────────────
typedef struct {
    PGM_P msg;
    uint32_t ms;
} TEST;
TEST result[12];

#define RUNTEST(n, str, test) { result[n].msg = PSTR(str); result[n].ms = test; delay(500); }

void runtests(void) {
    uint8_t i, len = 24;
    uint32_t total = 0;
    
    RUNTEST(0,  "FillScreen               ", testFillScreen());
    RUNTEST(1,  "Text                     ", testText());
    RUNTEST(2,  "Lines                    ", testLines(CYAN));
    RUNTEST(3,  "Horiz/Vert Lines         ", testFastLines(RED, BLUE));
    RUNTEST(4,  "Rectangles (outline)     ", testRects(GREEN));
    RUNTEST(5,  "Rectangles (filled)      ", testFilledRects(YELLOW, MAGENTA));
    RUNTEST(6,  "Circles (filled)         ", testFilledCircles(10, MAGENTA));
    RUNTEST(7,  "Circles (outline)        ", testCircles(10, WHITE));
    RUNTEST(8,  "Triangles (outline)      ", testTriangles());
    RUNTEST(9,  "Triangles (filled)       ", testFilledTriangles());
    RUNTEST(10, "Rounded rects (outline)  ", testRoundRects());
    RUNTEST(11, "Rounded rects (filled)   ", testFilledRoundRects());

    tft.fillScreen(BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(0, 0);
    uint16_t wid = tft.width();
    if (wid > 176) {
        tft.setTextSize(2);
#if defined(MCUFRIEND_KBV_H_)
        tft.print("MCUFRIEND ");
#if MCUFRIEND_KBV_H_ != 0
        tft.print(0.01 * MCUFRIEND_KBV_H_, 2);
#else
        tft.print("for");
#endif
        tft.println(" UNO");
#else
        tft.println("Adafruit-Style Tests");
#endif
    } else {
        len = wid / 6 - 8;
    }
    tft.setTextSize(1);

    for (i = 0; i < 12; i++) {
        PGM_P str = result[i].msg;
        char c;
        if (len > 24) {
            if (i < 10) tft.print(" ");
            tft.print(i);
            tft.print(": ");
        }
        uint8_t cnt = len;
        while ((c = pgm_read_byte(str++)) && cnt--) {
            tft.print(c);
        }
        tft.print(" ");
        tft.println(result[i].ms);
        total += result[i].ms;
    }

    tft.setTextSize(2);
    tft.print("Total:");
    tft.print(0.000001 * total);
    tft.println(" sec");

    tft.print("ID: 0x");
    tft.println(tft.readID(), HEX);
    tft.print("F_CPU:");
    tft.print(0.000001 * F_CPU);
#if defined(__OPTIMIZE_SIZE__)
    tft.println(" MHz -Os");
#else
    tft.println(" MHz");
#endif

    delay(10000);
}

// ─── การทดสอบกราฟิกย่อย ─────────────────────────────────────────

// FillScreen
unsigned long testFillScreen() {
    unsigned long start = micros();
    tft.fillScreen(BLACK);
    tft.fillScreen(RED);
    tft.fillScreen(GREEN);
    tft.fillScreen(BLUE);
    tft.fillScreen(BLACK);
    return micros() - start;
}

// Text
unsigned long testText() {
    unsigned long start;
    tft.fillScreen(BLACK);
    start = micros();
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);  tft.setTextSize(1);
    tft.println("Hello World!");
    tft.setTextColor(YELLOW); tft.setTextSize(2);
    tft.println(123.45);
    tft.setTextColor(RED);    tft.setTextSize(3);
    tft.println(0xDEADBEEF, HEX);
    tft.println();
    tft.setTextColor(GREEN);
    tft.setTextSize(5);
    tft.println("Groop");
    tft.setTextSize(2);
    tft.println("I implore thee,");
    tft.setTextSize(1);
    tft.println("my foonting turlingdromes.");
    tft.println("And hooptiously drangle me");
    tft.println("with crinkly bindlewurdles,");
    tft.println("Or I will rend thee");
    tft.println("in the gobberwarts");
    tft.println("with my blurglecruncheon,");
    tft.println("see if I don't!");
    return micros() - start;
}

// Lines
unsigned long testLines(uint16_t color) {
    unsigned long start, t;
    int x1, y1, x2, y2,
        w = tft.width(),
        h = tft.height();

    tft.fillScreen(BLACK);

    x1 = y1 = 0;
    y2 = h - 1;
    start = micros();
    for (x2 = 0; x2 < w; x2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    x2 = w - 1;
    for (y2 = 0; y2 < h; y2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    t = micros() - start; // fillScreen ไม่ถูกนับเวลาที่นี่

    tft.fillScreen(BLACK);

    x1 = w - 1;
    y1 = 0;
    y2 = h - 1;
    start = micros();
    for (x2 = 0; x2 < w; x2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    x2 = 0;
    for (y2 = 0; y2 < h; y2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    t += micros() - start;

    tft.fillScreen(BLACK);

    x1 = 0;
    y1 = h - 1;
    y2 = 0;
    start = micros();
    for (x2 = 0; x2 < w; x2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    x2 = w - 1;
    for (y2 = 0; y2 < h; y2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    t += micros() - start;

    tft.fillScreen(BLACK);

    x1 = w - 1;
    y1 = h - 1;
    y2 = 0;
    start = micros();
    for (x2 = 0; x2 < w; x2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    x2 = 0;
    for (y2 = 0; y2 < h; y2 += 6) tft.drawLine(x1, y1, x2, y2, color);
    t += micros() - start;

    return t;
}

// FastLines
unsigned long testFastLines(uint16_t color1, uint16_t color2) {
    unsigned long start;
    int x, y, w = tft.width(), h = tft.height();

    tft.fillScreen(BLACK);
    start = micros();
    for (y = 0; y < h; y += 5) tft.drawFastHLine(0, y, w, color1);
    for (x = 0; x < w; x += 5) tft.drawFastVLine(x, 0, h, color2);

    return micros() - start;
}

// Rect (outline)
unsigned long testRects(uint16_t color) {
    unsigned long start;
    int n, i, i2,
        cx = tft.width()  / 2,
        cy = tft.height() / 2;

    tft.fillScreen(BLACK);
    n = min(tft.width(), tft.height());
    start = micros();
    for (i = 2; i < n; i += 6) {
        i2 = i / 2;
        tft.drawRect(cx - i2, cy - i2, i, i, color);
    }

    return micros() - start;
}

// FilledRect
unsigned long testFilledRects(uint16_t color1, uint16_t color2) {
    unsigned long start, t = 0;
    int n, i, i2,
        cx = tft.width()  / 2 - 1,
        cy = tft.height() / 2 - 1;

    tft.fillScreen(BLACK);
    n = min(tft.width(), tft.height());
    for (i = n; i > 0; i -= 6) {
        i2 = i / 2;
        start = micros();
        tft.fillRect(cx - i2, cy - i2, i, i, color1);
        t += micros() - start;
        tft.drawRect(cx - i2, cy - i2, i, i, color2);
    }

    return t;
}

// FilledCircles
unsigned long testFilledCircles(uint8_t radius, uint16_t color) {
    unsigned long start;
    int x, y, w = tft.width(), h = tft.height(), r2 = radius * 2;

    tft.fillScreen(BLACK);
    start = micros();
    for (x = radius; x < w; x += r2) {
        for (y = radius; y < h; y += r2) {
            tft.fillCircle(x, y, radius, color);
        }
    }

    return micros() - start;
}

// Circles
unsigned long testCircles(uint8_t radius, uint16_t color) {
    unsigned long start;
    int x, y, r2 = radius * 2,
         w = tft.width()  + radius,
         h = tft.height() + radius;

    start = micros();
    for (x = 0; x < w; x += r2) {
        for (y = 0; y < h; y += r2) {
            tft.drawCircle(x, y, radius, color);
        }
    }

    return micros() - start;
}

// Triangles
unsigned long testTriangles() {
    unsigned long start;
    int n, i,
        cx = tft.width()  / 2 - 1,
        cy = tft.height() / 2 - 1;

    tft.fillScreen(BLACK);
    n = min(cx, cy);
    start = micros();
    for (i = 0; i < n; i += 5) {
        tft.drawTriangle(
            cx    , cy - i,  // top
            cx - i, cy + i,  // bottom-left
            cx + i, cy + i,  // bottom-right
            tft.color565(0, 0, i));
    }

    return micros() - start;
}

// FilledTriangles
unsigned long testFilledTriangles() {
    unsigned long start, t = 0;
    int i,
        cx = tft.width()  / 2 - 1,
        cy = tft.height() / 2 - 1;

    tft.fillScreen(BLACK);
    start = micros();
    for (i = min(cx, cy); i > 10; i -= 5) {
        start = micros();
        tft.fillTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                         tft.color565(0, i, i));
        t += micros() - start;
        tft.drawTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                         tft.color565(i, i, 0));
    }

    return t;
}

// RoundRects (outline)
unsigned long testRoundRects() {
    unsigned long start;
    int w, i, i2, red, step,
        cx = tft.width()  / 2 - 1,
        cy = tft.height() / 2 - 1;

    tft.fillScreen(BLACK);
    w = min(tft.width(), tft.height());
    start = micros();
    red = 0;
    step = (256 * 6) / w;
    for (i = 0; i < w; i += 6) {
        i2 = i / 2;
        red += step;
        tft.drawRoundRect(cx - i2, cy - i2, i, i, i / 8, tft.color565(red, 0, 0));
    }

    return micros() - start;
}

// FilledRoundRects
unsigned long testFilledRoundRects() {
    unsigned long start;
    int i, i2, green, step,
        cx = tft.width()  / 2 - 1,
        cy = tft.height() / 2 - 1;

    tft.fillScreen(BLACK);
    start = micros();
    green = 256;
    step = (256 * 6) / min(tft.width(), tft.height());
    for (i = min(tft.width(), tft.height()); i > 20; i -= 6) {
        i2 = i / 2;
        green -= step;
        tft.fillRoundRect(cx - i2, cy - i2, i, i, i / 8, tft.color565(0, green, 0));
    }

    return micros() - start;
}
