#include "board.h"
#include <stdio.h>
/* 在Keil Watch中观察g_measurement，屏幕与调试器使用相同数据。 */
typedef struct {
    uint16_t adc_vin,adc_current,adc_vref,adc_iset;
    uint32_t vdd_mv,vin_mv,current_ma,power_mw;
    int16_t temperature_d10;
    uint16_t humidity_d10;
    bool adc_ok,sensor_ok,range_ok,charge_requested;
} Measurement;
extern volatile Measurement g_measurement;
volatile Measurement g_measurement;
static bool average(unsigned ch,uint16_t *v) {
    uint16_t x;uint32_t sum=0;
    if(!adc_read(ch,&x)) return false; /* 换通道后丢弃首样本 */
    for(unsigned i=0;i<16u;++i) {if(!adc_read(ch,&x)) return false;sum+=x;}
    *v=(uint16_t)((sum+8u)/16u);return true;
}
static bool voltage_in_range(void) {
    return g_measurement.adc_vin < 4000u;
}

static bool current_adc_in_range(void) {
    return g_measurement.adc_current < 4000u;
}

static bool current_in_range(void) {
    return current_adc_in_range() &&
           g_measurement.current_ma <= CURRENT_VALID_MAX_MA;
}

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
    uint32_t vdd=3000u*cal/ref;
    if(vdd<2700u||vdd>3600u) {g_measurement.adc_ok=false;return;}
    uint32_t imv=(uint32_t)i*vdd/4095u;
    uint32_t ma=imv>CURRENT_ZERO_MV?(imv-CURRENT_ZERO_MV)*1000u/(SHUNT_MOHM*CURRENT_GAIN):0u;
    g_measurement.vdd_mv=vdd;
    g_measurement.vin_mv=(uint32_t)v*vdd*VOLTAGE_DIV_NUM/(4095u*VOLTAGE_DIV_DEN);
    g_measurement.current_ma=ma;
    g_measurement.power_mw=g_measurement.vin_mv*ma/1000u;
    g_measurement.range_ok = voltage_in_range() && current_in_range();
}
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
/* 首轮可用数据建立状态。无效或接近 ADC 满量程的轮次
   中断确认，保留上次已确认状态。 */
static bool current_display_over_range(bool valid, bool over_range) {
    static bool initialized;
    static bool confirmed;
    static uint8_t pending;

    if (!valid) {
        /* 这次读数无效，清零连续计数，保留上次确认的状态。 */
        pending = 0u;
    } else if (!initialized) {
        /* 上电后的第一笔有效读数直接决定初始状态。 */
        initialized = true;
        confirmed = over_range;
    } else if (over_range == confirmed) {
        /* 读数回到当前状态，之前累计的次数作废。 */
        pending = 0u;
    } else if (++pending == 3u) {
        /* 上一行先把计数加 1。满 3 次才切换状态。 */
        confirmed = over_range;
        pending = 0u;
    }
    return confirmed;
}

/* 左列显示板端输入，右列显示环境数据和充电开关状态。 */
static void render(void) {
    char voltage[12], current[12], power[12], temperature[12], humidity[12];
    bool can_show_current = g_measurement.adc_ok && current_adc_in_range();
    bool current_alert = current_display_over_range(can_show_current, !current_in_range());
    OledValue cells[OLED_FIELD_COUNT] = {
        [OLED_VOLTAGE] = {voltage, OLED_INVALID},
        [OLED_CURRENT] = {current, OLED_INVALID},
        [OLED_POWER] = {power, OLED_INVALID},
        [OLED_TEMPERATURE] = {temperature, OLED_INVALID},
        [OLED_HUMIDITY] = {humidity, OLED_INVALID},
        [OLED_CHARGE] = {NULL, g_measurement.charge_requested ? OLED_REQUEST : OLED_CLOSED},
    };

    if (g_measurement.adc_ok) {
        uint32_t voltage_cv = (g_measurement.vin_mv + 5u) / 10u;
        uint32_t power_cw = (g_measurement.power_mw + 5u) / 10u;
        snprintf(voltage, sizeof voltage, "%lu.%02luV",
                 (unsigned long)(voltage_cv / 100u), (unsigned long)(voltage_cv % 100u));
        snprintf(current, sizeof current, "%lumA", (unsigned long)g_measurement.current_ma);
        snprintf(power, sizeof power, "%lu.%02luW",
                 (unsigned long)(power_cw / 100u), (unsigned long)(power_cw % 100u));
        cells[OLED_VOLTAGE].state = voltage_in_range() ? OLED_NUMBER : OLED_OVER_RANGE;
        if (can_show_current) {
            cells[OLED_CURRENT].state = current_alert ? OLED_NUMBER_ALERT : OLED_NUMBER;
        } else {
            cells[OLED_CURRENT].state = OLED_OVER_RANGE;
        }
        if (g_measurement.range_ok) {
            cells[OLED_POWER].state = OLED_NUMBER;
        }
    }

    if (g_measurement.sensor_ok) {
        int32_t t = g_measurement.temperature_d10;
        int32_t absolute_t = t < 0 ? -t : t;
        snprintf(temperature, sizeof temperature, "%s%ld.%ld^C",
                 t < 0 ? "-" : "", (long)(absolute_t / 10), (long)(absolute_t % 10));
        snprintf(humidity, sizeof humidity, "%u.%u%%",
                 g_measurement.humidity_d10 / 10u, g_measurement.humidity_d10 % 10u);
        cells[OLED_TEMPERATURE].state = OLED_NUMBER;
        cells[OLED_HUMIDITY].state = OLED_NUMBER;
    }

    display_dashboard(cells);
}
#endif

int main(void) {
    board_init();charge_set(false);display_init();sensor_init();
    for(;;) {
        uint32_t start=system_ms; measure();
        int16_t t=0;uint16_t h=0;g_measurement.sensor_ok=sensor_read(&t,&h);
        if(g_measurement.sensor_ok) {g_measurement.temperature_d10=t;g_measurement.humidity_d10=h;}
        /* 请求开启不等于正在充电。无充电状态采样，不显示“已充满”。 */
        bool allow=g_measurement.adc_ok&&g_measurement.range_ok&&g_measurement.vin_mv>=4400u&&g_measurement.vin_mv<=5900u;
        g_measurement.charge_requested=allow && (CHARGE_ENABLE != 0);
        charge_set(allow);render();
#if BUZZER_ENABLE
        bool fault=!g_measurement.adc_ok||!g_measurement.range_ok||!g_measurement.sensor_ok;
        if(fault) {beep_set(true);delay_ms(40);beep_set(false);}
#endif
        uint32_t elapsed=system_ms-start;if(elapsed<1000u)delay_ms(1000u-elapsed);
    }
}


