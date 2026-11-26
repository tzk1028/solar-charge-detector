#include "board.h"
/* 自制5x7字形；每字节是一列，最低位在顶部。仅使用数字/大写字母。 */
static const uint8_t glyphs[][5]={
{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},
{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},
{0x3E,0x41,0x49,0x49,0x7A},{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},
{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},
{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},
{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,8,0x14,0x63},
{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};
static uint8_t column(char c,unsigned x) {
    if(x>=5u) return 0;
    if(c>='0'&&c<='9') return glyphs[c-'0'][x];
    if(c>='A'&&c<='Z') return glyphs[10+c-'A'][x];
    if(c=='.') return x==2u?0x40:0;
    if(c=='-') return 8;
    if(c==':') return x==2u?0x24:0;
    if(c=='%') { const uint8_t p[5]={0x63,0x13,8,0x64,0x63}; return p[x]; }
    return 0;
}
/* 上面的字形表保存数字和英文字母的点阵。这里先保留 TFT 的备用显示代码，当前工程会编译下面的 OLED 分支。 */
#if DISPLAY_TFT
static void spi(uint8_t v) {
    for(unsigned i=0;i<8u;++i) {
        pin_write(GPIOA,1,false); pin_write(GPIOA,2,(v&0x80u)!=0);
        __NOP();__NOP(); pin_write(GPIOA,1,true); __NOP();__NOP(); v=(uint8_t)(v<<1);
    }
    pin_write(GPIOA,1,false);
}
static void cmd(uint8_t c) { pin_write(GPIOA,6,false); pin_write(GPIOA,4,false); spi(c); pin_write(GPIOA,6,true); }
static void data(uint8_t d) { pin_write(GPIOA,6,false); pin_write(GPIOA,4,true); spi(d); pin_write(GPIOA,6,true); }
static void pair(uint16_t v) { data((uint8_t)(v>>8)); data((uint8_t)v); }
static void window(unsigned x,unsigned y,unsigned w,unsigned h) {
    cmd(0x2A); pair((uint16_t)(x+TFT_X_OFFSET)); pair((uint16_t)(x+w-1u+TFT_X_OFFSET));
    cmd(0x2B); pair((uint16_t)(y+TFT_Y_OFFSET)); pair((uint16_t)(y+h-1u+TFT_Y_OFFSET)); cmd(0x2C);
}
void display_init(void) {
    const unsigned pins[]={1,2,4,5,6};
    for(unsigned i=0;i<5u;++i) { pin_write(GPIOA,pins[i],true); pin_mode(GPIOA,pins[i],1,false); }
    pin_write(GPIOA,5,false); delay_ms(20); pin_write(GPIOA,5,true); delay_ms(120);
    cmd(0x01); delay_ms(150); cmd(0x11); delay_ms(120);
    cmd(0xB1); data(0x01);data(0x2C);data(0x2D);
    cmd(0xB2); data(0x01);data(0x2C);data(0x2D);
    cmd(0xB3); data(0x01);data(0x2C);data(0x2D);data(0x01);data(0x2C);data(0x2D);
    cmd(0xB4); data(0x07);
    cmd(0xC0);data(0xA2);data(0x02);data(0x84);
    cmd(0xC1);data(0xC5);cmd(0xC2);data(0x0A);data(0x00);
    cmd(0xC3);data(0x8A);data(0x2A);cmd(0xC4);data(0x8A);data(0xEE);
    cmd(0xC5);data(0x0E);cmd(0x36);data(TFT_MADCTL);cmd(0x3A);data(0x05);
    cmd(TFT_INVERT?0x21:0x20);cmd(0x13);cmd(0x29);delay_ms(20);
    window(0,0,80,160);
    pin_write(GPIOA,6,false);pin_write(GPIOA,4,true);
    for(unsigned i=0;i<80u*160u;++i) { spi(0);spi(0); }
    pin_write(GPIOA,6,true);pin_write(GPIOA,7,true);
    /* 四边白框便于检查偏移/裁切。 */
    for(unsigned y=0;y<160u;++y) { window(0,y,80,1);pin_write(GPIOA,6,false);pin_write(GPIOA,4,true);
        for(unsigned x=0;x<80u;++x) { uint8_t v=(x==0u||x==79u||y==0u||y==159u)?255u:0u;spi(v);spi(v); }
        pin_write(GPIOA,6,true);
    }
}
void display_line(unsigned row,const char *s) {
    if(row>=13u) return;
    char text[14];unsigned len=0; while(len<13u&&s[len]) {text[len]=s[len];++len;} while(len<13u) text[len++]=' ';
    window(1,2u+row*12u,78,8);pin_write(GPIOA,6,false);pin_write(GPIOA,4,true);
    for(unsigned y=0;y<8u;++y) for(unsigned x=0;x<78u;++x) {
        bool on=(column(text[x/6u],x%6u)&(1u<<y))!=0;spi(on?0xFF:0);spi(on?0xFF:0);
    } pin_write(GPIOA,6,true);
}
#else
/* SSD1315，U9 的 SCL/SDA 分别接 PA1/PA2。 */
#define OLED_ADDRESS 0x3Cu
#include "oled_font.h"

/* “太阳能充电检测装置”的固定点阵。一个字节对应同一列的八个像素。 */
static const uint8_t oled_title[2][129] = {
    {
        0x40,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA0, 0x7C, 0xA0, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x00, 0xFC, 0x04, 0xF4, 0x0C, 0x00, 0xFC, 0x84, 0x84, 0x84, 0x84, 0xFC, 0x00, 0x20,
        0xB0, 0xA8, 0xA4, 0xA8, 0xB0, 0x40, 0x7C, 0x48, 0x48, 0x48, 0x44, 0x64, 0x10, 0x10, 0x90, 0x90,
        0x50, 0x3C, 0x18, 0x10, 0x10, 0x50, 0x90, 0x10, 0x10, 0x00, 0xF0, 0x90, 0x90, 0x90, 0x90, 0xFC,
        0x90, 0x90, 0x90, 0xF0, 0x00, 0x00, 0x20, 0xA0, 0xFC, 0x20, 0x20, 0x80, 0xE0, 0xB0, 0x8C, 0x98,
        0xA0, 0x40, 0x80, 0x00, 0x64, 0xC8, 0x10, 0xFC, 0x04, 0xF4, 0x04, 0xFC, 0x00, 0xF8, 0x00, 0xFC,
        0x00, 0x88, 0x58, 0x30, 0xFC, 0x00, 0xD0, 0x50, 0x50, 0x7C, 0x50, 0x50, 0x50, 0x80, 0xBC, 0x94,
        0x94, 0x9C, 0x94, 0xF4, 0x94, 0x9C, 0x94, 0x94, 0xBC, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x40,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x30, 0x10, 0x08, 0x04, 0x0B, 0x10, 0x11, 0x06, 0x08, 0x10,
        0x30, 0x20, 0x00, 0x3F, 0x08, 0x08, 0x0F, 0x00, 0x3F, 0x10, 0x10, 0x10, 0x10, 0x3F, 0x00, 0x00,
        0x3F, 0x0A, 0x2A, 0x2A, 0x3F, 0x00, 0x3F, 0x24, 0x24, 0x22, 0x22, 0x39, 0x20, 0x21, 0x11, 0x19,
        0x07, 0x01, 0x01, 0x01, 0x3F, 0x21, 0x21, 0x22, 0x38, 0x00, 0x0F, 0x04, 0x04, 0x04, 0x04, 0x3F,
        0x24, 0x24, 0x24, 0x27, 0x20, 0x18, 0x06, 0x01, 0x3F, 0x01, 0x22, 0x24, 0x28, 0x20, 0x22, 0x2C,
        0x38, 0x26, 0x20, 0x00, 0x38, 0x0E, 0x20, 0x13, 0x08, 0x07, 0x08, 0x13, 0x20, 0x27, 0x20, 0x3F,
        0x10, 0x11, 0x09, 0x09, 0x3D, 0x23, 0x13, 0x15, 0x09, 0x19, 0x15, 0x25, 0x21, 0x20, 0x20, 0x3F,
        0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x3F, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
};

/* OLED 把屏幕分成 8 页，每页高 8 个像素。先发命令选页、把列位置移到开头，再发一页图像数据。 */
static bool oled_page(unsigned page, const uint8_t data[129]) {
    const uint8_t address[] = {0x00, (uint8_t)(0xB0u + page), 0x00, 0x10};

    if (!i2c_write(OLED_ADDRESS, address, sizeof address)) {
        return false;
    }
    return i2c_write(OLED_ADDRESS, data, 129u);
}

/* 切换到屏幕的 I2C 引脚。温湿度传感器有自己的引脚，处理完传感器再画屏时要切回来。 */
static void oled_bus(void) {
    i2c_init(GPIOA, 1, GPIOA, 2);
}

void display_init(void) {
    /* 先关闭显示，配置页寻址和电荷泵，写好显存后点亮。 */
    const uint8_t setup[] = {
        0x00, 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F,
        0xD9, 0xF1, 0xDB, 0x30, 0xA4, 0xA6, 0x8D, 0x14
    };
    /* 0x40 是 OLED 的数据标记，后面 128 个字节各写一列。初始化时顶端两页放标题，其余页先清空。 */
    const uint8_t blank[129] = {0x40};
    const uint8_t on[] = {0x00, 0xAF};

    delay_ms(200);
    oled_bus();
    if (!i2c_write(OLED_ADDRESS, setup, sizeof setup)) {
        return;
    }
    for (unsigned page = 0; page < 8u; ++page) {
        const uint8_t *data = page < 2u ? oled_title[page] : blank;
        if (!oled_page(page, data)) {
            return;
        }
    }
    (void)i2c_write(OLED_ADDRESS, on, sizeof on);
}

/* 16行高的缓冲分两页。每页第0字节是 I2C 数据标识。 */
typedef uint8_t OledRow[2][129];

/* 把一列 16 像素拆成上、下两组各 8 位。两个数组的第 0 个字节都留给 I2C 数据标记。 */
static void oled_column(OledRow row, unsigned x, uint16_t pixels) {
    row[0][x + 1u] = (uint8_t)pixels;
    row[1][x + 1u] = (uint8_t)(pixels >> 8);
}

/* 每个汉字占 12 列。从字形表逐列取像素，第二个字紧跟在第一个右边。 */
static void oled_word(OledRow row, unsigned x, Hanzi first, Hanzi second) {
    for (unsigned col = 0; col < 12u; ++col) {
        oled_column(row, x + col, hanzi[first][col]);
        oled_column(row, x + 12u + col, hanzi[second][col]);
    }
}

static uint8_t number_column(char c, unsigned x) {
    if (c == 'm') {
        static const uint8_t letter_m[] = {0x7C, 0x04, 0x38, 0x04, 0x7C};
        return letter_m[x];
    }
    return column(c, x);
}

/* 把字符串逐字符转为点阵，数字字形在竖直方向放大。下一个字符超出格子边界时停止绘制。 */
static void oled_number(OledRow row, unsigned x, unsigned limit, const char *text) {
    static const uint8_t degree[] = {0x06, 0x09, 0x06};
    for (; *text != '\0'; ++text) {
        char c = *text;
        unsigned width = 5u;
        if (c == '.' || c == '^') {
            width = 3u;
        } else if (c == '-') {
            width = 4u;
        }

        if (x + width > limit) {
            break;
        }
        for (unsigned col = 0; col < width; ++col) {
            uint16_t bits = 0;
            if (c == '^') { /* 与后面的 C 组成 °C。 */
                bits = (uint16_t)(degree[col] << 2);
            } else if (c == '.') {
                bits = col == 1u ? 0x1800u : 0u;
            } else {
                uint8_t original = number_column(c, col);
                for (unsigned y = 0; y < 11u; ++y) {
                    if ((original & (1u << (y * 7u / 11u))) != 0u) {
                        bits |= (uint16_t)(1u << (y + 2u));
                    }
                }
            }
            oled_column(row, x + col, bits);
        }
        x += width + (c == '.' ? 0u : 1u);
    }
}

/* 数据区有 3 行、2 列，每行占 2 页。主程序传来每格的状态，这里决定画数字还是状态词。 */
void display_dashboard(const OledValue values[OLED_FIELD_COUNT]) {
    static const Hanzi labels[OLED_FIELD_COUNT][2] = {
        [OLED_VOLTAGE] = {HZ_DIAN, HZ_YA},
        [OLED_TEMPERATURE] = {HZ_WEN, HZ_DU},
        [OLED_CURRENT] = {HZ_DIAN, HZ_LIU},
        [OLED_HUMIDITY] = {HZ_SHI, HZ_DU},
        [OLED_POWER] = {HZ_GONG, HZ_LV},
        [OLED_CHARGE] = {HZ_CHONG, HZ_DIAN},
    };

    oled_bus();
    for (unsigned line = 0; line < 3u; ++line) {
        OledRow row = {{0x40}, {0x40}};
        oled_column(row, 63u, 0x5555u);
        for (unsigned col = 0; col < 2u; ++col) {
            unsigned field = line * 2u + col;
            unsigned left = col * 64u;
            unsigned start = left + 27u;
            oled_word(row, left + 1u, labels[field][0], labels[field][1]);
            switch (values[field].state) {
                case OLED_NUMBER:
                    oled_number(row, start, left + 63u, values[field].text);
                    break;
                case OLED_OVER_RANGE:
                    oled_word(row, start, HZ_CHAO, HZ_XIAN);
                    break;
                case OLED_CLOSED:
                    oled_word(row, start, HZ_GUAN, HZ_BI);
                    break;
                case OLED_REQUEST:
                    oled_word(row, start, HZ_QING, HZ_QIU);
                    break;
                case OLED_INVALID:
                    oled_word(row, start, HZ_WU, HZ_XIAO);
                    break;
            }
        }
        /* 从第 2 页开始写数据区，把第 0、1 页的标题留在屏幕上。 */
        unsigned page = 2u + line * 2u;
        if (!oled_page(page, row[0]) || !oled_page(page + 1u, row[1])) {
            return;
        }
    }
}
#endif
