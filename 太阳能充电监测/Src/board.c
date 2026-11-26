#include "board.h"
/* 这一文件直接和 STM32 外设寄存器打交道。应用代码通过这里的函数读 ADC、控制引脚和等待时间。 */
uint32_t SystemCoreClock = 16000000u;
volatile uint32_t system_ms;
void SystemInit(void) { /* 复位默认HSI16，不启用PLL和外部晶振。 */ }
/* SysTick 每毫秒触发一次中断，CPU 暂停眼前的代码执行这里，把计时变量加 1。 */
void SysTick_Handler(void) { ++system_ms; }
/* p 选 GPIO 端口，n 选端口上的引脚。BSRR 的低 16 位负责置高，高 16 位负责拉低。 */
void pin_write(GPIO_TypeDef *p, unsigned n, bool v) { p->BSRR = 1u << (n + (v ? 0u : 16u)); }
bool pin_read(GPIO_TypeDef *p, unsigned n) { return (p->IDR & (1u << n)) != 0u; }
/*
 * 每个引脚在 MODER 占两位。先用掩码清除旧设置，再写新模式。这里用到 1（输出）和 3（模拟）。
 * od 为 true 时选开漏输出。它能主动拉低引脚，释放后由外部上拉电阻拉高。
 */
void pin_mode(GPIO_TypeDef *p, unsigned n, unsigned m, bool od) {
    p->MODER = (p->MODER & ~(3u << (n*2u))) | (m << (n*2u));
    p->PUPDR &= ~(3u << (n*2u));
    if (od) p->OTYPER |= 1u<<n; else p->OTYPER &= ~(1u<<n);
}
/* 用当前时刻减开始时刻计算已等待多久。__WFI 让 CPU 等中断，毫秒计时仍会继续。 */
void delay_ms(uint32_t ms) { uint32_t t=system_ms; while ((uint32_t)(system_ms-t)<ms) __WFI(); }
void delay_us(uint32_t us) { /* TIM14以1MHz自由运行，16位差值支持短延时。 */
    while (us) { uint16_t n=(uint16_t)(us>30000u?30000u:us); uint16_t t=(uint16_t)TIM14->CNT;
        while ((uint16_t)((uint16_t)TIM14->CNT-t)<n) {} us-=n; }
}
/* 主程序发出开启请求后，还要经过各自的配置总开关。配置为 0 时对应引脚保持低电平。 */
void charge_set(bool on) { pin_write(GPIOA,11,on && (CHARGE_ENABLE != 0)); }
void beep_set(bool on) { pin_write(GPIOA,3,on && (BUZZER_ENABLE != 0)); }
static bool wait_clear(volatile uint32_t *reg,uint32_t mask) {
    uint32_t t=system_ms; while (*reg & mask) if ((uint32_t)(system_ms-t)>10u) return false; return true;
}
static bool adc_ready;
void board_init(void) {
    /* 先给 GPIO 外设打开时钟，寄存器才可用。后面同样要给定时器和 ADC 开时钟。 */
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
    /* 先打开 ADC 内部稳压，再校准、启用内部参考并等待就绪。初始化超时就提前返回，采样也会报失败。 */
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
/*
 * 一次采样的次序是选通道、等通道配置生效、启动转换、等完成、读结果。
 * 等待有超时。同步或转换失败时将 ADC 标为未就绪，本次运行后续采样也会返回失败。
 */
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

