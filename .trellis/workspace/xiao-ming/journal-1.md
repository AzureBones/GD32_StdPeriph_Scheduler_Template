# Journal - xiao-ming (Part 1)

> AI development session journal
> Started: 2026-05-27

---



## Session 1: 移植 DMA+IDLE+RingBuffer 串口接收方案

**Date**: 2026-05-27
**Task**: 移植 DMA+IDLE+RingBuffer 串口接收方案
**Branch**: `master`

### Summary

(Add summary)

### Main Changes

## 完成内容

将 HAL 库工程中的 DMA + 串口空闲中断 + RingBuffer 接收方案移植到 GD32 标准库工程。

| 改动 | 说明 |
|------|------|
| `usart_app.h` | 新增 `ring_buffer.h` 引用、`extern uart0_rb`、`app_ringbuffer_init()` 声明 |
| `usart_app.c` | 移除 `rx_flag`/`uart_dma_buffer`，新增 `ring_buffer_t uart0_rb`，`uart_task()` 改为从 RingBuffer 读取 |
| `gd32f4xx_it.c` | ISR 中用 `ring_buffer_write()` 替换 `memcpy + rx_flag`，移除废弃 extern |
| `main.c` | APP 层 init 区增加 `app_ringbuffer_init()` |
| `Project.uvprojx` | Keil 工程加入 `Components/ringbuffer/ring_buffer.c`，Include Paths 加入 `../Components/ringbuffer` |
| `.gitignore` | 忽略 Keil 编译产物、用户配置文件、AI 工具目录 |
| `spec/backend/directory-structure.md` | 记录工程分层架构与初始化模式 |
| `spec/backend/quality-guidelines.md` | 记录 DMA+IDLE+RingBuffer 标准方案与禁止的 rx_flag 方案 |

**验证**：Keil 编译 0 错误 0 警告，断点确认 `rx_len = 5`（发送 `hhh\r\n`），回显正常。


### Git Commits

| Hash | Message |
|------|---------|
| `066790f` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete

---

## Session 2: 移植定时器触发 DMA 块处理 ADC 方案

**Date**: 2026-05-28
**Task**: adc-timer-dma-port
**Branch**: `master`

### Summary

将 HAL 参考工程的 ADC Mode 3（TIMER TRGO 硬件触发 + DMA Normal 模式 + DMA 完成中断 + 500组批量处理）移植到 GD32F470VET6 标准外设库工程。期间发现并修复了"DMA 仅触发一次"的隐式假设 bug：HAL 的 Stop/Start DMA 封装了两层操作（ADC 外设 DMA 位 + DMA 控制器），仅重启 DMA 控制器不够，必须同时循环 ADC 侧 DMA 模式位。

### Main Changes

| 文件 | 改动说明 |
|------|---------|
| `mcu_cmic_gd32f470vet6.c` | `adc_value[2]→[1000]`，新增 `adc_dma_done` 标志，DMA 改 Normal 模式 + FTF 中断，ADC 改 TIMER2 TRGO 外部触发，新增 `timer2_adc_config()`（PSC=119, ARR=99, 10kHz） |
| `gd32f4xx_it.c` | 新增 `DMA1_Channel0_IRQHandler`：双层重启（`adc_dma_mode_disable` → DMA restart → `adc_dma_mode_enable`）|
| `adc_app.c` | 改为标志位驱动批处理，`extern adc_value[1000]` |
| `oled_app.c` | 同步 `extern adc_value[1000]` 声明 |
| `spec/backend/quality-guidelines.md` | 新增 ADC DMA Normal 模式标准模板、双层重启规则、新增功能检查清单 |

### Key Technical Decisions

- **TIMER2** 选用（空闲的通用定时器），`ADC_EXTTRIG_ROUTINE_T2_TRGO` 对应 HAL 的 TIM3_TRGO
- **双层重启顺序**：① `adc_dma_mode_disable` ② `dma_channel_disable` ③ `dma_transfer_number_config(1000)` ④ `dma_channel_enable` ⑤ `adc_dma_mode_enable`
- 时钟链：SYSCLK 240MHz → APB1 /4 → 60MHz → TIMER2 ×2 → **120MHz**，PSC=119, ARR=99 → **10kHz**

### Git Commits

| Hash | Message |
|------|---------|
| `ae46131` | feat(adc): port timer-triggered DMA block mode from HAL to GD32 stdlib |

### Testing

- [OK] DMA FTF 中断每 ~50ms 循环触发（Keil 断点验证）
- [OK] `adc_value[0..4]` 数据在 0~4095 范围内
- [OK] OLED A0 值随变阻器正确变化
- [NOTE] CH12（PC2/Vref）引脚未接线，悬空读噪声属正常，接 TL431 基准电路跳线帽后可稳定

### Status

[OK] **Completed**

### Next Steps

- 为 CH12 安装跳线帽，连接板上 TL431 ADC基准电路（页3，ADC基准区域 2-pin 插针）
