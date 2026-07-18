# verify-dac7311idckr-demo

基于 STM32F103 + RT-Thread 的 DAC7311IDCKR SPI DAC 验证工程，用于泵控/模拟输出场景的硬件功能验证。

## 概述

本项目通过软件 SPI（GPIO Bit-Bang）驱动 TI DAC7311IDCKR 14-bit 单通道 DAC，支持电压设定、原始值写入、百分比控制和省电模式，配合 RT-Thread Shell 进行交互式调试验证。

## 硬件信息

| 项目 | 说明 |
|------|------|
| MCU | STM32F103（72MHz，Cortex-M0） |
| DAC | TI DAC7311IDCKR（14-bit，单通道，SPI） |
| VREF | 5.0V（VDD） |
| 输出范围 | 0 ~ 5.0V（实测线性） |

### 引脚连接

| STM32 引脚 | DAC7311 引脚 | 功能 |
|------------|-------------|------|
| PB7 | SYNC | 片选（低电平有效） |
| PB8 | SCLK | SPI 时钟 |
| PB9 | DIN | SPI 数据输入 |

### DAC7311 SPI 协议

- **模式**：SPI Mode 0（CPOL=0, CPHA=0），MSB First
- **帧格式**：16-bit `[X X PD1 PD0 D13 D12 ... D0]`
- **满量程**：0x3FFF（16383）
- **输出公式**：`VOUT = VREF × D / 16384`

## 工程结构

```
verify-dac7311idckr-demo/
├── applications/
│   ├── main.c                    # 应用入口 + RT-Thread Shell 命令
│   └── macSYS/
│       ├── Inc/
│       │   ├── bsp_sys.h         # BSP 系统配置
│       │   └── dac7311.h         # DAC7311 驱动头文件
│       └── Src/
│           ├── bsp_sys.c         # BSP 系统实现
│           └── dac7311.c         # DAC7311 驱动实现（GPIO Bit-Bang SPI）
├── docs/
│   └── 我的项目概要.md
├── images/
│   └── dac7311_schemtic.png      # 原理图
├── rt-thread/                    # RT-Thread 内核 + 组件
├── LICENSE                       # Apache 2.0
└── README.md
```

## 驱动 API

```c
/* 初始化 DAC7311 GPIO，输出 0V */
void dac7311_init(void);

/* 设定输出电压（0.0 ~ 5.0V），自动钳位 */
void dac7311_set_voltage(float voltage);

/* 设定 14-bit 原始值（0 ~ 16383） */
void dac7311_set_raw(uint16_t value);

/* 设定百分比（0 ~ 100%），映射到 0 ~ VREF */
void dac7311_set_percent(uint8_t percent);

/* 设置省电模式（Normal / 1k / 100k / HiZ） */
void dac7311_power_down(uint8_t mode);

/* 获取当前输出电压 */
float dac7311_get_voltage(void);
```

## Shell 调试命令

串口波特率 115200，连接后可使用以下命令：

```bash
# 设定电压
dac volt 2.5          # 输出 2.5V

# 设定原始值
dac raw 8191          # 输出约 2.5V（16383/2）

# 设定百分比
dac pct 50            # 输出 50% = 2.5V

# 省电模式
dac pd 0              # 正常模式
dac pd 1              # 1k 下拉省电
dac pd 2              # 100k 下拉省电
dac pd 3              # 高阻态

# 查看状态
dac info
```

### 输出示例

```
[DAC7311] Initialized (open-drain + 5V pull-up), output=0.00V
[main] DAC7311 verification demo started
[main] Use shell command: dac <voltage|raw|pct|pd> <value>
[main] DAC output set to 2.500V
msh >dac info
[DAC7311 Status]
  Voltage: 2.500V
  Pinout:  PB7=SYNC, PB8=SCLK, PB9=DIN
  VREF:    5.0V (VDD)
  Mode:    SPI SW Bit-Bang, Mode 0, MSB first
```

## 原理图

![DAC7311 原理图](images/dac7311_schemtic.png)

## 构建环境

- **IDE**：RT-Thread Studio / Keil MDK
- **RTOS**：RT-Thread（标准版）
- **芯片**：STM32F103 系列

## License

[Apache License 2.0](LICENSE)
