#include "oled.h"
#include "main.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>

#define OLED_ADDRESS 0x78U
#define OLED_WIDTH 128U
#define OLED_COLUMN_OFFSET 2U
#define OLED_SCL(state) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, (state))
#define OLED_SDA(state) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, (state))

static uint8_t framebuffer[8][OLED_WIDTH];

/* Same setup and orientation as the supplied normal-display SH1106 example. */
static const uint8_t init_commands[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0x66, 0xD9, 0xF1, 0xDB, 0x30, 0xA4, 0xA6
};

/* Five columns per glyph; bit 0 is the top pixel. */
static const char glyph_names[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-%";
static const uint8_t glyphs[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
    {0x00,0x36,0x36,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x23,0x13,0x08,0x64,0x62}
};

static void DelayUs(uint16_t us)
{
    uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < us) {}
}

static uint8_t ClockHigh(void)
{
    uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
    OLED_SCL(GPIO_PIN_SET);
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET)
    {
        if ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) >= 100U)
            return 0;
    }
    DelayUs(5);
    return 1;
}

static void Stop(void)
{
    OLED_SCL(GPIO_PIN_RESET);
    OLED_SDA(GPIO_PIN_RESET);
    DelayUs(5);
    (void)ClockHigh();
    OLED_SDA(GPIO_PIN_SET);
    DelayUs(5);
}

static uint8_t Start(void)
{
    OLED_SDA(GPIO_PIN_SET);
    if (!ClockHigh() || HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_RESET)
        return 0;
    OLED_SDA(GPIO_PIN_RESET);
    DelayUs(5);
    OLED_SCL(GPIO_PIN_RESET);
    DelayUs(5);
    return 1;
}

static uint8_t WriteByte(uint8_t value)
{
    uint8_t bit, ack;
    for (bit = 0; bit < 8; bit++)
    {
        OLED_SDA((value & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        DelayUs(5);
        if (!ClockHigh()) return 0;
        OLED_SCL(GPIO_PIN_RESET);
        DelayUs(5);
        value <<= 1;
    }
    OLED_SDA(GPIO_PIN_SET);
    DelayUs(5);
    if (!ClockHigh()) return 0;
    ack = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_RESET);
    OLED_SCL(GPIO_PIN_RESET);
    DelayUs(5);
    return ack;
}

static uint8_t Write(uint8_t control, const uint8_t *data, uint16_t count)
{
    uint16_t i;
    uint8_t ok = Start();
    if (ok) ok = WriteByte(OLED_ADDRESS);
    if (ok) ok = WriteByte(control);
    for (i = 0; ok && i < count; i++) ok = WriteByte(data[i]);
    Stop();
    return ok;
}

static uint8_t Refresh(void)
{
    uint8_t page;
    for (page = 0; page < 8; page++)
    {
        uint8_t position[3];
        position[0] = (uint8_t)(0xB0U + page);
        position[1] = 0x10U | (OLED_COLUMN_OFFSET >> 4);
        position[2] = OLED_COLUMN_OFFSET & 0x0FU;
        if (!Write(0x00, position, sizeof(position)) ||
            !Write(0x40, framebuffer[page], OLED_WIDTH)) return 0;
    }
    return 1;
}

static void DrawText(uint8_t x, uint8_t y, const char *text, uint8_t scale)
{
    uint8_t column, row, dx, dy;
    while (*text && (uint16_t)x + 6U * scale <= OLED_WIDTH)
    {
        const char *found = strchr(glyph_names, *text++);
        if (found != NULL)
        {
            const uint8_t *glyph = glyphs[found - glyph_names];
            for (column = 0; column < 5; column++)
            {
                for (row = 0; row < 7; row++)
                {
                    if ((glyph[column] & (1U << row)) == 0) continue;
                    for (dx = 0; dx < scale; dx++)
                    {
                        for (dy = 0; dy < scale; dy++)
                        {
                            uint16_t py = y + row * scale + dy;
                            if (py < 64U)
                                framebuffer[py / 8U][x + column * scale + dx] |=
                                    (uint8_t)(1U << (py % 8U));
                        }
                    }
                }
            }
        }
        x = (uint8_t)(x + 6U * scale);
    }
}

uint8_t OLED_Init(void)
{
    uint8_t i;
    const uint8_t display_on = 0xAF;
    OLED_SDA(GPIO_PIN_SET);
    /* Release a slave left mid-byte if the MCU was reset during a transfer. */
    for (i = 0; i < 9; i++)
    {
        OLED_SCL(GPIO_PIN_RESET);
        DelayUs(5);
        if (!ClockHigh()) { Stop(); return 0; }
    }
    Stop();
    HAL_Delay(100);
    if (!Write(0x00, init_commands, sizeof(init_commands))) return 0;
    memset(framebuffer, 0, sizeof(framebuffer));
    DrawText(0, 0, "DHT11 MONITOR", 1);
    DrawText(0, 24, "WAITING", 2);
    if (!Refresh()) return 0;
    return Write(0x00, &display_on, 1);
}

uint8_t OLED_ShowReadings(uint8_t temperature, uint8_t humidity, uint8_t valid)
{
    char line[16];
    memset(framebuffer, 0, sizeof(framebuffer));
    DrawText(0, 0, "DHT11 MONITOR", 1);
    if (valid)
    {
        (void)snprintf(line, sizeof(line), "TEMP:%uC", (unsigned int)temperature);
        DrawText(0, 16, line, 2);
        (void)snprintf(line, sizeof(line), "HUM :%u%%", (unsigned int)humidity);
        DrawText(0, 36, line, 2);
        DrawText(0, 56, "READ OK", 1);
    }
    else
    {
        DrawText(0, 16, "TEMP:--C", 2);
        DrawText(0, 36, "HUM :--%", 2);
        DrawText(0, 56, "DHT11 READ ERROR", 1);
    }
    return Refresh();
}
