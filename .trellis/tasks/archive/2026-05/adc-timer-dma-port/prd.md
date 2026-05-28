# brainstorm: 移植定时器触发 + DMA块处理 ADC方案到 GD32 标准库

## Goal

将 HAL 参考工程（GD32_Xifeng_ADDA）中的 ADC Mode 3（TIM3 TRGO 硬件触发 + DMA Normal 模式 + DMA完成中断 + 批量缓冲处理）移植到本工程（GD32F470VET6 标准外设库）中。
当前本工程使用软件触发 + DMA循环模式，每次只保存最新的2个采样值，无法做批量/定时采样。

## What I already know

- HAL Mode 3: TIM3_TRGO → ADC1双通道扫描 → DMA Normal 1000样本缓冲 → DMA完成中断 → 批量处理 → 重启DMA
- 本工程当前方案: 软件触发 → ADC0双通道(CH10/CH12) → DMA1-CH0 循环模式 → adc_value[2] 实时覆盖
- GD32F470 ADC 支持外部定时器触发（T0/T1/T2/T3/T4/T7）
- GD32 标准库 DMA 支持 FTF（Full Transfer Finish）中断：`DMA_INT_FTF`
- GD32 DMA 支持 Normal 模式：`DMA_CIRCULAR_MODE_DISABLE`
- 项目中已占用定时器：TIMER5（DAC触发），其余均空闲
- `ADC_EXTTRIG_ROUTINE_T2_TRGO` 可触发 ADC0（等同 STM32 TIM3_TRGO）
- `DMA1_Channel0_IRQHandler` 当前未实现，需新增

## Assumptions (temporary)

- 目标采样通道保持不变（CH10 + CH12）
- 采样缓冲区大小待定（HAL参考用1000，本工程需根据需求确定）
- 定时器选用 TIMER2（TRGO 触发，空闲）

## Open Questions

- 块缓冲区大小设置多少？（影响中断频率和内存占用）
- DMA完成后数据如何处理？（只是打印？还是写入其他缓冲区？）

## Requirements (evolving)

- ADC0 改为外部定时器硬件触发（TIMER2 TRGO）
- DMA1-CH0 改为 Normal 模式（非循环），传满后触发中断
- 新增 `DMA1_Channel0_IRQHandler`，在中断中处理数据并重启DMA
- 增大 `adc_value` 缓冲区以支持批量数据
- `adc_task()` 改为标志位驱动（中断置标志，任务处理数据）

## Acceptance Criteria (evolving)

- [ ] TIMER2 正确初始化并以目标频率触发 ADC 转换
- [ ] ADC0 连续扫描2个通道，由 TIMER2 TRGO 触发
- [ ] DMA 传满缓冲区后触发中断（不再循环自动覆盖）
- [ ] 中断处理函数能正确清除标志、重启DMA
- [ ] `adc_task()` 在数据就绪标志置位时处理批量数据

## Technical Notes

### 可行性评估：高度可行

| HAL 组件 | GD32 标准库等效 | 状态 |
|---------|----------------|------|
| TIM3 TRGO | TIMER2 TRGO (`ADC_EXTTRIG_ROUTINE_T2_TRGO`) | 可用（TIMER2空闲） |
| DMA Normal 模式 | `dma_circulation_disable(DMA1, DMA_CH0)` | API 已存在 |
| DMA完成中断 | `DMA_INT_FTF`, `DMA1_Channel0_IRQHandler` | API存在，handler待添加 |
| ADC外部触发使能 | `adc_external_trigger_config(..., EXTERNAL_TRIGGER_RISING)` | 已配置触发源，只需使能 |
| 块缓冲区 | 扩大 `adc_value[]` 至 N*2 | 简单修改 |
| DMA重启 | clear flag → config number → enable channel | USART DMA中已有相同pattern |

### 需要修改的文件

- `Code/Components/bsp/mcu_cmic_gd32f470vet6.c`：
  - `bsp_adc_init()`：改触发源、使能外部触发、关DMA循环、开DMA中断、配NVIC
  - 新增 `bsp_timer2_for_adc_init()`
  - 扩大 `adc_value[]` 数组
- `Code/USER/src/gd32f4xx_it.c`：新增 `DMA1_Channel0_IRQHandler`
- `Code/APP/adc_app.c`：改为标志位驱动的批量处理

### DMA重启 pattern（来自项目USART中断）

```c
dma_flag_clear(DMA1, DMA_CH0, DMA_FLAG_FTF);
dma_transfer_number_config(DMA1, DMA_CH0, BUFFER_SIZE);
dma_channel_enable(DMA1, DMA_CH0);
```
