#ifndef BOARD_H
#define BOARD_H
#include "stm32g031xx.h"
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
extern volatile uint32_t system_ms;
void SysTick_Handler(void);
void board_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);
void pin_write(GPIO_TypeDef *port, unsigned pin, bool value);
bool pin_read(GPIO_TypeDef *port, unsigned pin);
void pin_mode(GPIO_TypeDef *port, unsigned pin, unsigned mode, bool open_drain);
void charge_set(bool on);
void beep_set(bool on);
bool adc_read(unsigned channel, uint16_t *value);
void i2c_init(GPIO_TypeDef *sclport, unsigned scl, GPIO_TypeDef *sdaport, unsigned sda);
bool i2c_write(uint8_t address, const uint8_t *data, unsigned count);
bool i2c_read(uint8_t address, uint8_t *data, unsigned count);
void display_init(void);
void display_line(unsigned row, const char *text);
/* OLED 字段按从左到右、从上到下排列。 */
typedef enum {
    OLED_VOLTAGE, OLED_TEMPERATURE,
    OLED_CURRENT, OLED_HUMIDITY,
    OLED_POWER, OLED_CHARGE,
    OLED_FIELD_COUNT
} OledField;

typedef enum { OLED_NUMBER, OLED_INVALID, OLED_OVER_RANGE, OLED_CLOSED, OLED_REQUEST, OLED_NUMBER_ALERT } OledValueState;

typedef struct {
    const char *text;
    uint32_t state; /* OledValueState，固定字宽与指针对齐。 */
} OledValue;

void display_dashboard(const OledValue values[OLED_FIELD_COUNT]);
void sensor_init(void);
bool sensor_read(int16_t *temperature, uint16_t *humidity);
#endif

