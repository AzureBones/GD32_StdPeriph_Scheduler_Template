# GD32F470 V2 Standalone App Template With Dependence

这是 V2 硬件的独立 App 模板工程，使用 AC6 + CMSIS6，并已随工程携带常用依赖，适合直接打开 Keil 工程开始开发。

## 工程信息

| 项 | 内容 |
| --- | --- |
| 硬件 | MICU / CMIC GD32F470VET6 V2 |
| 工具链 | MDK-ARM AC6 |
| CMSIS | CMSIS6 |
| 入口工程 | `MDK/Project.uvprojx` |
| 启动地址 | `0x08000000` |
| 适用场景 | 普通裸机 App 开发，不带 BootLoader / OTA |

## 目录说明

| 目录 | 内容 |
| --- | --- |
| `APP/` | 应用层示例：LED、按键、OLED、串口、SD 卡、ADC、RTC 和简易调度器。 |
| `Components/` | 板级 BSP、FatFs、按键库、SPI Flash、GD30AD3344、OLED、SDIO 等组件。 |
| `Driver/` | CMSIS6 相关文件。 |
| `Libraries/` | GD32F4xx 标准外设库和启动文件。 |
| `PACK/` | 工程内置扩展包，例如 perf_counter 2.5.4。 |
| `USER/` | `main.c`、中断文件、SysTick 等用户入口代码。 |
| `MDK/` | Keil 工程文件、调试配置和输出目录。 |

## 板级功能

- GD32F470VET6，Cortex-M4，最高 200 MHz
- 6 路用户 LED，V2 为高电平点亮
- 6 路用户按键和 1 路唤醒/复位相关按键
- 0.91 英寸 SSD1306 OLED，I2C0
- GD25Qxx SPI Flash
- GD30AD3344 SPI 外设
- SDIO SD 卡和 FatFs
- USART0 debug 串口，另预留 USART1、USART2、USART5 配置
- ADC、DAC、RTC 示例
- V2 引脚映射包含 SDIO、以太网、SPI、USART、ADC、DAC 等板级资源
- perf_counter 性能计数器

## 编译和下载

1. 打开 `MDK/Project.uvprojx`。
2. 在 Keil 中执行 `Project -> Rebuild all target files`。
3. 使用调试器下载并运行。

本工程从 Flash 起始地址 `0x08000000` 运行。如果后续要移植到 BootLoader 场景，需要修改 scatter、IROM 起始地址和 `SCB->VTOR`。

## 开发提示

- 外设引脚集中定义在 `Components/bsp/mcu_cmic_gd32f470vet6.h`。
- 新增应用任务时，在 `APP/` 下添加模块，并在 `scheduler.c` 中注册。
- 使用 DMA 前先参考仓库根目录 `doc/DMA_CHANNEL_MAP.md`，避免通道冲突。
- 编译输出、临时文件和 Keil 用户配置不建议提交。
