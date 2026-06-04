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

---

## Session 3: Shell 移植 — LittleFS Shell → FATFS，GD32 标准库

**Date**: 2026-06-01
**Branch**: `master`
**Commits**: `4eb87af`

### 工作内容

将 `GD32_Xifeng_ADDA` HAL 工程中基于 LittleFS 的交互式 Shell 移植到本 GD32 标准库工程，并全量对接 FATFS 文件系统（SD 卡）。

### Git Commits

| Hash | Message |
|------|---------|
| `4eb87af` | feat(shell): port LittleFS shell to FATFS for GD32 stdlib |

### 变更文件

| 文件 | 变更 |
|------|------|
| `Code/APP/shell_app.c` | 新增（754 行）：FATFS Shell 实现 |
| `Code/APP/shell_app.h` | 新增：Shell 公共接口声明 |
| `Code/APP/usart_app.c` | 修改：`uart_task()` 调用 `shell_process()` |
| `Code/USER/src/main.c` | 修改：添加 `shell_init()` 调用 |
| `Code/Components/bsp/mcu_cmic_gd32f470vet6.h` | 修改：注册 `shell_app.h` |
| `Code/MDK/Project.uvprojx` | 修改：加入 `shell_app.c` |

### 技术要点

**LittleFS → FatFs R0.09 API 转换**：

| LittleFS | FatFs R0.09 |
|----------|-------------|
| `lfs_dir_open/read/close` | `f_opendir/f_readdir`（无 `f_closedir`）|
| `lfs_file_open/read/write/close` | `f_open/f_read/f_write/f_close` |
| `lfs_mkdir/remove/rename` | `f_mkdir/f_unlink/f_rename` |
| `LFS_ERR_OK` | `FR_OK` |
| `info.type == LFS_TYPE_DIR` | `info.fattrib & AM_DIR` |
| 路径 `"/dir"` | 路径 `"0:/dir"`（加驱动器号前缀）|

**Shell 接入**：`uart_task()` → `ring_buffer_read()` → `shell_process(buf, len)`，无需新增调度任务。

**初始化顺序**：`shell_init()` 必须在 `sd_fatfs_test()`（FATFS 挂载）之后调用。

### 过程排查

- 源文件遗留 `#include "mydefine.h"` → 删除（HAL 工程专用头文件）
- `shell_state_t` 遗留 `lfs_t *fs` 字段 → 全量替换为新版头文件
- `f_closedir` 不存在于 FatFs R0.09 → 删除全部调用
- `shell_app.h` 应注册到 `mcu_cmic_gd32f470vet6.h`（工程主头文件约定）

### 沉淀到 spec

- `error-handling.md`：Flash 掉电测试正确方法、SD 卡 `while(1)` bug 修复、坏 TF 卡诊断、`0xFF` printf 陷阱
- `quality-guidelines.md`：Shell/uart_task 集成模式、FatFs R0.09 四个陷阱
- `directory-structure.md`：主头文件约定、Shell 初始化顺序

### Testing

- [OK] Shell Banner 正常显示：`== FATFS Shell v1.0 ==`
- [OK] `help` 命令列出全部 13 条命令
- [OK] `ls` 列出 SD 卡根目录（`FATFS.TXT`、`PROJEC~1.TXT`、`System Volume Information`）
- [OK] FATFS 对接验证通过

### Status

[OK] **Completed**


## Session 4: 同步上游模板 Bug 修复 — BSP 重命名、GD30 SPI DMA、ADC 漂移

**Date**: 2026-06-04
**Task**: 同步上游模板 Bug 修复 — BSP 重命名、GD30 SPI DMA、ADC 漂移
**Branch**: `master`

### Summary

(Add summary)

### Main Changes

| 变更类别 | 详情 |
|---------|------|
| BSP 重命名 | mcu_cmic → mcu_cimc（typo 修正），14 处 include + uvprojx 全部同步 |
| gd30ad3344 SPI 修复 | DMA 通道从硬编码 DMA1/CH3/CH4 改为宏 GD30_DMA/CH0/CH1，解决 V2 引脚冲突 |
| gd30ad3344 API 重构 | 新增 OS/Mode/DR/PullUp/NOP 枚举；修复 ADS118_PGA_SET double-if bug；修复符号扩展 |
| BSP 深睡眠修复 | bsp_spi_disable_for_deepsleep 替换为 GD30_CS_HIGH() 和 GD30_DMA_CHANNEL_TX 宏 |
| ADC 漂移修复 | adc_task() 末尾追加 dac_data_set()，修复双通道采集偏移 |
| main.c | 注释掉 test_spi_flash() 调用 |

**测试结果**:
- Boot 日志完整，GD30 init 输出 0x4443（DMA 通道新配置验证通过）
- Shell 13 条命令全部正常，FATFS 读写通过
- PA4 (DAC0_OUT0) 跟随电位器变化，漂移修复生效
- Deep Sleep 进出正常（OLED/GD30/USART 均重初始化）
- UART 长报文接收正常

**关键发现**:
- GD30 新 DMA 通道 (DMA1/CH0) 与 ADC DMA (DMA1/CH0) 共用同一通道，当前无冲突（调度器无 GD30 运行时读任务），但若未来添加 GD30 read task 需注意
- LED 深睡眠唤醒后不亮为预存 bug（led_disp static temp_old 变量未重置），与本次改动无关
- 原理图确认：PC0 接板载电位器 VR1，PC2 接 TL431 精密基准（约 3.3V），PA4 经 SENSOR H6 接口座引出

**Updated Files**:
- `Code/Components/bsp/mcu_cimc_gd32f470vet6.c` (renamed + deep sleep fix)
- `Code/Components/bsp/mcu_cimc_gd32f470vet6.h` (renamed + GD30_* macros)
- `Code/Components/gd30ad3344/gd30ad3344.c` (new DMA macros + bug fixes)
- `Code/Components/gd30ad3344/gd30ad3344.h` (new enums, remove hardcoded CS)
- `Code/APP/adc_app.c` (dac_data_set drift fix)
- `Code/USER/src/main.c` (comment test_spi_flash)
- `Code/MDK/Project.uvprojx` (BSP rename)
- 9x APP/*.c (include name update)


### Git Commits

| Hash | Message |
|------|---------|
| `61d0b1a` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
