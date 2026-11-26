#include "board.h"
uint32_t SystemCoreClock = 16000000u;
volatile uint32_t system_ms;
void SystemInit(void) { /* 复位默认HSI16，不启用PLL和外部晶振。 */ }
void SysTick_Handler(void) { ++system_ms; }
void pin_write(GPIO_TypeDef *p, unsigned n, bool v) { p->BSRR = 1u << (n + (v ? 0u : 16u)); }
bool pin_read(GPIO_TypeDef *p, unsigned n) { return (p->IDR & (1u << n)) != 0u; }
void pin_mode(GPIO_TypeDef *p, unsigned n, unsigned m, bool od) {
    p->MODER = (p->MODER & ~(3u << (n*2u))) | (m << (n*2u));
    p->PUPDR &= ~(3u << (n*2u));
    if (od) p->OTYPER |= 1u<<n; else p->OTYPER &= ~(1u<<n);
}
void delay_ms(uint32_t ms) { uint32_t t=system_ms; while ((uint32_t)(system_ms-t)<ms) __WFI(); }
void delay_us(uint32_t us) { /* TIM14以1MHz自由运行，16位差值支持短延时。 */
    while (us) { uint16_t n=(uint16_t)(us>30000u?30000u:us); uint16_t t=(uint16_t)TIM14->CNT;
        while ((uint16_t)((uint16_t)TIM14->CNT-t)<n) {} us-=n; }
}
void charge_set(bool on) { pin_write(GPIOA,11,on && (CHARGE_ENABLE != 0)); }
void beep_set(bool on) { pin_write(GPIOA,3,on && (BUZZER_ENABLE != 0)); }
static bool wait_clear(volatile uint32_t *reg,uint32_t mask) {
    uint32_t t=system_ms; while (*reg & mask) if ((uint32_t)(system_ms-t)>10u) return false; return true;
}
static bool adc_ready;
void board_init(void) {
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;
    (void)RCC->IOPENR;
    /* 先写输出值再切模式，避免充电开关、蜂鸣器和背光毛刺。 */
    pin_write(GPIOA,11,false); pin_mode(GPIOA,11,1,false);
    pin_write(GPIOA,3,false); pin_mode(GPIOA,3,1,false);
    pin_write(GPIOA,7,false); pin_mode(GPIOA,7,1,false);
    pin_mode(GPIOA,0,3,false);  /* PA0只采样ISET，不输出也不加内部上拉 */
    pin_mode(GPIOA,12,3,false); /* 电流 ADC_IN16 */
    pin_mode(GPIOB,0,3,false);  /* 电压 ADC_IN8；共封装脚其他内部pad保持模拟 */
    SysTick_Config(SystemCoreClock/1000u);
    RCC->APBENR2 |= RCC_APBENR2_TIM14EN | RCC_APBENR2_ADCEN;
    TIM14->PSC=15u; TIM14->ARR=65535u; TIM14->EGR=1u; TIM14->CR1=1u;
    ADC1->CR |= ADC_CR_ADVREGEN; delay_ms(1);
    ADC1->CFGR2 = ADC_CFGR2_CKMODE_0; /* PCLK/2=8MHz */
    ADC1->CR |= ADC_CR_ADCAL;
    if (!wait_clear(&ADC1->CR,ADC_CR_ADCAL)) return;
    delay_us(10);
    ADC1->SMPR=7u; /* 最大采样时间，利于分压和内部参考稳定 */
    ADC1_COMMON->CCR |= ADC_CCR_VREFEN; delay_ms(1);
    ADC1->ISR=ADC_ISR_ADRDY; ADC1->CR |= ADC_CR_ADEN;
    uint32_t t=system_ms;
    while (!(ADC1->ISR&ADC_ISR_ADRDY)) if ((uint32_t)(system_ms-t)>10u) return;
    adc_ready=true;
}
bool adc_read(unsigned ch,uint16_t *v) {
    if (!adc_ready || ch>18u) return false;
    ADC1->ISR=ADC_ISR_CCRDY;
    ADC1->CHSELR=1u<<ch;
    /* G0的通道配置需要跨时钟域同步；等待CCRDY后再启动。 */
    uint32_t config_start=system_ms;
    while (!(ADC1->ISR&ADC_ISR_CCRDY))
        if ((uint32_t)(system_ms-config_start)>10u) { adc_ready=false; return false; }
    ADC1->ISR=ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
    ADC1->CR |= ADC_CR_ADSTART;
    uint32_t t=system_ms;
    while (!(ADC1->ISR&ADC_ISR_EOC)) if ((uint32_t)(system_ms-t)>10u) {
        ADC1->CR |= ADC_CR_ADSTP; adc_ready=false; return false;
    }
    *v=(uint16_t)ADC1->DR; return true;
}

