# teaching version

这是太阳能充电监测的 OLED 中文教学版，基于 630 mA 固件的原始源码。用 Keil 打开 [SolarDetector_OLED.uvprojx](Keil/SolarDetector_OLED.uvprojx)，按下面的顺序读即可。

1. 从 [Src/main.c](Src/main.c) 底部的 `main()` 开始。它约每秒做一轮测量、状态判断和屏幕刷新。
2. 回到同一文件的 `measure()`。顺着 `average()` 看 16 次 ADC 读数如何求平均，再追踪 `current_ma` 怎样从引脚电压换算成 mA。
3. 找到 `current_in_range()` 和 OLED 分支的 `render()`。假设 ADC 原始值小于 4000，试着判断 629、630、631 mA 三组输入。预期是前两组画数字，最后一组在同一轮显示「超限」。
4. 接着看 [Src/display.c](Src/display.c) 的 `display_dashboard()`。它把数字和状态词画成像素，经 I²C 发到 OLED。
5. 想了解芯片怎么读到原始数据，再看 [Src/board.c](Src/board.c) 的 `adc_read()`、[Src/sensor.c](Src/sensor.c) 的 `sensor_read()`，以及 [Inc/config.h](Inc/config.h) 中的参数和默认开关。

学习时可以在 Keil 的 Watch 窗口观察 `g_measurement.adc_current`、`g_measurement.current_ma` 和 `g_measurement.range_ok`，分别对应原始刻度、换算后的电流和本轮量程判断。

## 工程和烧录信息

解压后双击 Keil/SolarDetector_OLED.uvprojx。目标芯片为 STM32G031F8P6，使用 Arm Compiler 6 和 Keil STM32G0xx_DFP 支持包。源码、头文件、启动文件和链接配置均随包提供。

原发布 ZIP 包含已编译的 solar_detector_oled.hex。从教学分支打开 Keil 工程后，可重新构建生成烧录文件。在 Keil 中可选择 ST-Link Debugger、SWD，使用 Flash → Download。现有工程的 U9 接线依次为 GND、3.3V、SCL 和 SDA。OLED 为 128×64 I²C 屏，使用 0x3C 地址。若旧版 Keil 在其他电脑上遇到目录编码问题，可把解压后的工程文件夹改成英文名。

OLED布局预览.png 使用示例数据。实物上方固定为「太阳能充电检测装置」。左列显示板端输入电压、电流、功率，单位为 V、mA、W。右列显示环境温度、相对湿度和充电开关状态。

- 「充电 关闭」表示本固件未开启充电支路。
- 「充电 请求」表示程序发出开启请求，硬件的实际充电状态需另行测量。
- 「超限」表示电压或电流采样超过当前可用范围。电流的软件显示阈值为 630 mA，原始 ADC 读数达到 4000 时仍判超限。「无效」表示本轮相关数值无法使用。

烧录前按板子接线确认 H1 的 GND、SWDIO 和 SWCLK。H1 第 4 针为 VIN。首次联调先用板上 USB-C 供电，电池暂不接入。

本版默认关闭蜂鸣器，OLED 保留异常提示。已完成编译及版式预览。实物屏幕显示、测量精度和烧录结果仍需上板验证。
