#ifndef CONFIG_H
#define CONFIG_H
/* 工程单位：mV、mA、mW、0.1摄氏度、0.1%RH。 */
#ifndef DISPLAY_TFT
#define DISPLAY_TFT 0
#endif
#define TFT_X_OFFSET 24u /* 80x160 面板在控制器RAM中的起点，须用边框图实测 */
#define TFT_Y_OFFSET 0u
#define TFT_MADCTL 0x08u
#define TFT_INVERT 1
#define BUZZER_ENABLE 0 /* 默认静音。OLED 仍显示故障提示。 */
#define CHARGE_ENABLE 0 /* ISET/按键网络与电池规格核实后才允许改成1 */
#define SHUNT_MOHM 100u
#define CURRENT_GAIN 50u
#define CURRENT_ZERO_MV 0u /* 仅在测量链路有效后，依据零电流实测调整 */
#define CURRENT_VALID_MAX_MA 630u /* 临时软件提示阈值，不是硬件限流 */
#define VOLTAGE_DIV_NUM 2u
#define VOLTAGE_DIV_DEN 1u
#endif

