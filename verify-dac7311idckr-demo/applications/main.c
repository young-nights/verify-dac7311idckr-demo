/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-15     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "bsp_sys.h"
#include "dac7311.h"

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize DAC7311 driver (PB7=SYNC, PB8=SCLK, PB9=DIN) */
  dac7311_init();

  rt_kprintf("[main] DAC7311 verification demo started\n");
  rt_kprintf("[main] Use shell command: dac <voltage|raw|pct|pd> <value>\n");

  /* Default output 2.5V (50% of VREF) for quick verification */
  dac7311_set_voltage(2.5f);
  rt_kprintf("[main] DAC output set to 2.500V\n");

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      rt_thread_mdelay(1000);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* ============================================================================ *  RT-Thread Shell Command: dac
 *  Usage:
 *    dac volt <value>     - Set output voltage (0.0 ~ 5.0V)
 *    dac raw <value>      - Set raw 12-bit value (0 ~ 4095)
 *    dac pct <value>      - Set percentage (0 ~ 100%)
 *    dac pd <mode>        - Power-down (0=normal, 1=1k, 2=100k, 3=HiZ)
 *    dac info             - Show current status
 * ===========================================================================*/
static int dac(int argc, char **argv)
{
    if (argc < 2) {
        rt_kprintf("Usage:\n");
        rt_kprintf("  dac volt <0.0~5.0>    Set voltage\n");
        rt_kprintf("  dac raw  <0~4095>     Set raw value\n");
        rt_kprintf("  dac pct  <0~100>      Set percentage\n");
        rt_kprintf("  dac pd   <0~3>        Power-down mode\n");
        rt_kprintf("  dac info              Show status\n");
        return -RT_ERROR;
    }

    if (rt_strcmp(argv[1], "volt") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac volt <voltage>\n"); return -RT_ERROR; }
        float v = atof(argv[2]);
        dac7311_set_voltage(v);
        rt_kprintf("[DAC] Voltage set to %.3fV (raw=%d)\n", dac7311_get_voltage(), (int)(v / DAC7311_VREF * 4095));
    } else if (rt_strcmp(argv[1], "raw") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac raw <0~4095>\n"); return -RT_ERROR; }
        uint16_t val = atoi(argv[2]);
        dac7311_set_raw(val);
        rt_kprintf("[DAC] Raw set to %d (%.3fV)\n", val, dac7311_get_voltage());
    } else if (rt_strcmp(argv[1], "pct") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac pct <0~100>\n"); return -RT_ERROR; }
        uint8_t pct = atoi(argv[2]);
        dac7311_set_percent(pct);
        rt_kprintf("[DAC] Percent set to %d%% (%.3fV)\n", pct, dac7311_get_voltage());
    } else if (rt_strcmp(argv[1], "pd") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac pd <0~3>\n"); return -RT_ERROR; }
        uint8_t mode = atoi(argv[2]);
        dac7311_power_down(mode);
        rt_kprintf("[DAC] Power-down mode=%d\n", mode);
    } else if (rt_strcmp(argv[1], "info") == 0) {
        rt_kprintf("[DAC7311 Status]\n");
        rt_kprintf("  Voltage: %.3fV\n", dac7311_get_voltage());
        rt_kprintf("  Pinout:  PB7=SYNC, PB8=SCLK, PB9=DIN\n");
        rt_kprintf("  VREF:    %.1fV (VDD)\n", DAC7311_VREF);
        rt_kprintf("  Mode:    SPI SW Bit-Bang, Mode 0, MSB first\n");
    } else {
        rt_kprintf("Unknown command: %s\n", argv[1]);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(dac, DAC7311 control: dac volt/raw/pct/pd/info <value>);

/* ============================================================================ *  Diagnostic command: dacdiag
 *  Reads GPIOB registers and manually toggles pins for hardware verification.
 * ===========================================================================*/
static int dacdiag(int argc, char **argv)
{
    GPIO_TypeDef *port = GPIOB;

    rt_kprintf("=== DAC7311 GPIO Diagnostic ===\n");

    /* Check RCC clock enable bit for GPIOB */
    uint32_t apb2enr = RCC->APB2ENR;
    rt_kprintf("[RCC] APB2ENR    = 0x%08X\n", (unsigned)apb2enr);
    rt_kprintf("[RCC] GPIOB EN   = %s\n", (apb2enr & RCC_APB2ENR_IOPBEN) ? "YES" : "NO");

    /* Read GPIOB configuration registers */
    rt_kprintf("[GPIOB] CRL      = 0x%08X  (pins 0-7)\n", (unsigned)port->CRL);
    rt_kprintf("[GPIOB] CRH      = 0x%08X  (pins 8-15)\n", (unsigned)port->CRH);
    rt_kprintf("[GPIOB] IDR      = 0x%04X    (input state)\n", (unsigned)port->IDR);
    rt_kprintf("[GPIOB] ODR      = 0x%04X    (output state)\n", (unsigned)port->ODR);

    /* Decode pin modes from CRH (pins 8,9 are in CRH) */
    uint32_t crh = port->CRH;
    uint32_t mode8 = crh & 0xF;        /* PB8 CNF+MODE bits */
    uint32_t mode9 = (crh >> 4) & 0xF;  /* PB9 CNF+MODE bits */
    rt_kprintf("[PB8] CRH bits = 0x%X -> ", (unsigned)mode8);
    if ((mode8 & 0x3) == 0x0) rt_kprintf("Input\n");
    else if ((mode8 & 0x3) == 0x1) rt_kprintf("Output 10MHz %s\n", (mode8 & 0x4) ? "Open-Drain" : "Push-Pull");
    else if ((mode8 & 0x3) == 0x2) rt_kprintf("Output 2MHz %s\n", (mode8 & 0x4) ? "Open-Drain" : "Push-Pull");
    else if ((mode8 & 0x3) == 0x3) rt_kprintf("Output 50MHz %s\n", (mode8 & 0x4) ? "Open-Drain" : "Push-Pull");

    rt_kprintf("[PB9] CRH bits = 0x%X -> ", (unsigned)mode9);
    if ((mode9 & 0x3) == 0x0) rt_kprintf("Input\n");
    else if ((mode9 & 0x3) == 0x1) rt_kprintf("Output 10MHz %s\n", (mode9 & 0x4) ? "Open-Drain" : "Push-Pull");
    else if ((mode9 & 0x3) == 0x2) rt_kprintf("Output 2MHz %s\n", (mode9 & 0x4) ? "Open-Drain" : "Push-Pull");
    else if ((mode9 & 0x3) == 0x3) rt_kprintf("Output 50MHz %s\n", (mode9 & 0x4) ? "Open-Drain" : "Push-Pull");

    /* Manual pin toggle test */
    rt_kprintf("\n--- Manual Toggle Test (measure PB8/PB9 with multimeter) ---\n");

    /* Force PB8 HIGH */
    port->BSRR = GPIO_PIN_8;
    rt_kprintf("[TEST] PB8 = HIGH, PB9 = LOW\n");
    rt_thread_mdelay(2000);

    /* Force PB9 HIGH */
    port->BSRR = GPIO_PIN_9;
    port->BRR = GPIO_PIN_8;
    rt_kprintf("[TEST] PB8 = LOW, PB9 = HIGH\n");
    rt_thread_mdelay(2000);

    /* Restore idle */
    port->BSRR = GPIO_PIN_7;  /* SYNC HIGH */
    port->BRR = GPIO_PIN_8;   /* SCLK LOW */
    port->BRR = GPIO_PIN_9;   /* DIN LOW */
    rt_kprintf("[TEST] Restored idle state\n");

    return RT_EOK;
}
MSH_CMD_EXPORT(dacdiag, DAC7311 GPIO diagnostic: read registers + toggle test);
