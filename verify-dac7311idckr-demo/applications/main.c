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
