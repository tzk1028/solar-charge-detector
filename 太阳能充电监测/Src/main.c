#include "board.h"
#include <stdio.h>
/*
 * 先从文件底部的 main() 读起。每轮先测量，再判断状态，最后刷新屏幕。
 * Measurement 把同一轮的 ADC 原始值、换算结果和状态放在一起，供主循环、屏幕和调试器查看。
 */
/* 在Keil Watch中观察g_measurement，屏幕与调试器使用相同数据。 */
typedef struct {
    uint16_t adc_vin,adc_current,adc_vref,adc_iset;
    uint32_t vdd_mv,vin_mv,current_ma,power_mw;
    int16_t temperature_d10;
    uint16_t humidity_d10;
    bool adc_ok,sensor_ok,range_ok,charge_requested;
} Measurement;
extern volatile Measurement g_measurement;
/* volatile 提醒编译器保留对这个对象的读写，方便在 Keil 的 Watch 窗口观察。 */
volatile Measurement g_measurement;
/*
 * ADC 像一把电子刻度尺，把引脚上的电压转成数字。每次切换通道先丢弃第一笔，
 * 再取 16 笔求平均，让随机波动小一些。任一笔失败就返回 false，调用者本轮不使用结果。
 * 参数 v 是结果的存放地址，*v 表示把平均值写回调用者提供的变量。
 */
static bool average(unsigned ch,uint16_t *v) {
    uint16_t x;uint32_t sum=0;
    if(!adc_read(ch,&x)) return false; /* 换通道后丢弃首样本 */
    for(unsigned i=0;i<16u;++i) {if(!adc_read(ch,&x)) return false;sum+=x;}
    *v=(uint16_t)((sum+8u)/16u);return true;
}
/* 12 位 ADC 的刻度是 0 到 4095。本工程提前把 4000 及以上视为超出可用量程。 */
static bool voltage_in_range(void) {
    return g_measurement.adc_vin < 4000u;
}

/*
 * 电流要同时通过两关：原始 ADC 小于 4000，换算电流小于或等于 630 mA。
 * 例如原始 ADC 在量程内时，630 mA 可显示数字，631 mA 立即显示「超限」。
 */
static bool current_in_range(void) {
    return g_measurement.adc_current < 4000u &&
           g_measurement.current_ma <= CURRENT_VALID_MAX_MA;
}

/*
 * 先读内部参考和板端电压、电流、ISET。&& 从左往右检查，遇到 false 就停止。
 * ISET 原始值留给调试观察，计算板端输入电流用的是电流采样通道。
 */
static void measure(void) {
    uint16_t v=0,i=0,ref=0,iset=0;
    bool ok=average(13,&ref)&&average(8,&v)&&average(16,&i)&&average(0,&iset);
    g_measurement.adc_ok=ok;
    g_measurement.range_ok=false;
    if(!ok) return;
    g_measurement.adc_vref=ref;g_measurement.adc_vin=v;g_measurement.adc_current=i;g_measurement.adc_iset=iset;
    /* ST工厂在3.0V测得的VREFINT校准值；这是芯片内部参考，不是假定VDD精确3.3V。 */
    uint16_t cal=*(const uint16_t *)0x1FFF75AAu;
    if(ref==0u||cal==0u||cal>4095u) {g_measurement.adc_ok=false;return;}
    /* 工厂记录的是供电为 3000 mV 时的参考读数。用工厂读数除以当前读数的比例，估算此刻的 ADC 供电电压。 */
    uint32_t vdd=3000u*cal/ref;
    if(vdd<2700u||vdd>3600u) {g_measurement.adc_ok=false;return;}
    /*
     * 先把电流通道的 ADC 刻度换算成放大器输出的毫伏数，再扣除零点偏移。
     * 用增益和采样电阻反推电流。乘 1000 是因为电阻配置单位为毫欧，结果用毫安表示。
     */
    uint32_t imv=(uint32_t)i*vdd/4095u;
    uint32_t ma=imv>CURRENT_ZERO_MV?(imv-CURRENT_ZERO_MV)*1000u/(SHUNT_MOHM*CURRENT_GAIN):0u;
    g_measurement.vdd_mv=vdd;
    /* 引脚电压经过了分压，乘回分压比才得到板端输入电压。板端毫伏乘毫安再除以 1000 得到毫瓦。 */
    g_measurement.vin_mv=(uint32_t)v*vdd*VOLTAGE_DIV_NUM/(4095u*VOLTAGE_DIV_DEN);
    g_measurement.current_ma=ma;
    g_measurement.power_mw=g_measurement.vin_mv*ma/1000u;
    g_measurement.range_ok = voltage_in_range() && current_in_range();
}
/* 这是编译时选择。当前配置为 0，编译器会选用下面 #else 中的 OLED 分支。 */
#if DISPLAY_TFT
static void render(void) {
    char b[24];display_line(0,"SOLAR INPUT");
    if(g_measurement.adc_ok) {
        snprintf(b,sizeof b,"VIN %lu MV",(unsigned long)g_measurement.vin_mv);display_line(1,b);
        if(g_measurement.range_ok) {
            snprintf(b,sizeof b,"I %lu MA",(unsigned long)g_measurement.current_ma);display_line(2,b);
            snprintf(b,sizeof b,"P %lu MW",(unsigned long)g_measurement.power_mw);display_line(3,b);
        } else {display_line(2,"I OVER RANGE");display_line(3,"P INVALID");}
    } else {display_line(1,"ADC ERROR");display_line(2,"I INVALID");display_line(3,"P INVALID");}
    if(g_measurement.sensor_ok) {
        int32_t t=g_measurement.temperature_d10; bool neg=t<0;if(neg)t=-t;
        snprintf(b,sizeof b,"T %s%ld.%ld C",neg?"-":"",(long)(t/10),(long)(t%10));display_line(4,b);
        snprintf(b,sizeof b,"RH %u.%u %%",g_measurement.humidity_d10/10u,g_measurement.humidity_d10%10u);display_line(5,b);
    } else {display_line(4,"AHT20 ERROR");display_line(5,"RH INVALID");}
    display_line(6,g_measurement.charge_requested?"CHG REQUEST":"CHG DISABLED");
    display_line(7,"INPUT ONLY");display_line(8,"NOT BATTERY");
    snprintf(b,sizeof b,"RAW I %u",g_measurement.adc_current);display_line(9,b);
    display_line(10,g_measurement.adc_ok&&g_measurement.range_ok?"CHECK HW TEST":"CHECK ADC");
}
#else
/* OLED 每格都有文字和状态。先给各测量格设为「无效」，采样通过才填入可用数据。 */
/* 左列显示板端输入，右列显示环境数据和充电开关状态。 */
static void render(void) {
    char voltage[12], current[12], power[12], temperature[12], humidity[12];
    OledValue cells[OLED_FIELD_COUNT] = {
        [OLED_VOLTAGE] = {voltage, OLED_INVALID},
        [OLED_CURRENT] = {current, OLED_INVALID},
        [OLED_POWER] = {power, OLED_INVALID},
        [OLED_TEMPERATURE] = {temperature, OLED_INVALID},
        [OLED_HUMIDITY] = {humidity, OLED_INVALID},
        [OLED_CHARGE] = {NULL, g_measurement.charge_requested ? OLED_REQUEST : OLED_CLOSED},
    };

    if (g_measurement.adc_ok) {
        /* 把毫伏和毫瓦换成两位小数所需的整数刻度。先加 5 再除以 10，实现四舍五入。 */
        uint32_t voltage_cv = (g_measurement.vin_mv + 5u) / 10u;
        uint32_t power_cw = (g_measurement.power_mw + 5u) / 10u;
        snprintf(voltage, sizeof voltage, "%lu.%02luV",
                 (unsigned long)(voltage_cv / 100u), (unsigned long)(voltage_cv % 100u));
        snprintf(current, sizeof current, "%lumA", (unsigned long)g_measurement.current_ma);
        snprintf(power, sizeof power, "%lu.%02luW",
                 (unsigned long)(power_cw / 100u), (unsigned long)(power_cw % 100u));
        cells[OLED_VOLTAGE].state = voltage_in_range() ? OLED_NUMBER : OLED_OVER_RANGE;
        /* 问号表达式按本轮判断选一种状态。通过就画数字，否则画「超限」。 */
        cells[OLED_CURRENT].state = current_in_range() ? OLED_NUMBER : OLED_OVER_RANGE;
        if (g_measurement.range_ok) {
            cells[OLED_POWER].state = OLED_NUMBER;
        }
    }

    if (g_measurement.sensor_ok) {
        /* 温度的整数单位是 0.1 摄氏度，例如 -123 表示 -12.3°C。先单独保留负号，再拆整数和小数。 */
        int32_t t = g_measurement.temperature_d10;
        int32_t absolute_t = t < 0 ? -t : t;
        snprintf(temperature, sizeof temperature, "%s%ld.%ld^C",
                 t < 0 ? "-" : "", (long)(absolute_t / 10), (long)(absolute_t % 10));
        snprintf(humidity, sizeof humidity, "%u.%u%%",
                 g_measurement.humidity_d10 / 10u, g_measurement.humidity_d10 % 10u);
        cells[OLED_TEMPERATURE].state = OLED_NUMBER;
        cells[OLED_HUMIDITY].state = OLED_NUMBER;
    }

    /* 这里只准备各格内容。display.c 负责画成像素并通过 I2C 发给 OLED。 */
    display_dashboard(cells);
}
#endif

/*
 * 芯片启动后进入 main()。先初始化引脚、计时和 ADC，设充电输出关闭，再初始化屏幕和温湿度传感器。
 * for(;;) 表示一直重复：采样、读温湿度、判断充电允许条件、刷新 OLED。
 */
int main(void) {
    board_init();charge_set(false);display_init();sensor_init();
    for(;;) {
        uint32_t start=system_ms; measure();
        int16_t t=0;uint16_t h=0;g_measurement.sensor_ok=sensor_read(&t,&h);
        if(g_measurement.sensor_ok) {g_measurement.temperature_d10=t;g_measurement.humidity_d10=h;}
        /* 请求开启不等于正在充电。无充电状态采样，不显示“已充满”。 */
        /*
         * 所有条件同时成立才允许：ADC 正常，板端电压和电流在量程内，输入介于 4.4 V 和 5.9 V（含边界）。
         * charge_set() 还受 config.h 里的总开关约束。默认总开关为 0，因此实际输出保持关闭。
         */
        bool allow=g_measurement.adc_ok&&g_measurement.range_ok&&g_measurement.vin_mv>=4400u&&g_measurement.vin_mv<=5900u;
        g_measurement.charge_requested=allow && (CHARGE_ENABLE != 0);
        charge_set(allow);render();
#if BUZZER_ENABLE
        bool fault=!g_measurement.adc_ok||!g_measurement.range_ok||!g_measurement.sensor_ok;
        if(fault) {beep_set(true);delay_ms(40);beep_set(false);}
#endif
        /* 一轮不足 1000 毫秒就等完剩余时间。处理超过 1000 毫秒时立即开始下一轮。 */
        uint32_t elapsed=system_ms-start;if(elapsed<1000u)delay_ms(1000u-elapsed);
    }
}


