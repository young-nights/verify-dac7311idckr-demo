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
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 *  Waveform Generator (for oscilloscope verification)
 * ===========================================================================*/
#define WAVE_LUT_SIZE       256

typedef enum {
    WAVE_NONE = 0,
    WAVE_SIN,
    WAVE_SQUARE,
    WAVE_TRI,
    WAVE_SAW
} wave_type_t;

static uint16_t    s_wave_lut[WAVE_LUT_SIZE];
static uint32_t    s_wave_idx    = 0;
static wave_type_t s_wave_type   = WAVE_NONE;
static float       s_wave_freq   = 1.0f;
static float       s_wave_amp    = 2.5f;
static float       s_wave_offset = 2.5f;
static rt_timer_t  s_wave_timer  = RT_NULL;

/* Pre-compute LUT from waveform type */
static void wave_build_lut(wave_type_t type, float amp, float offset)
{
    for (int i = 0; i < WAVE_LUT_SIZE; i++) {
        float t = (float)i / (float)WAVE_LUT_SIZE;  /* 0.0 ~ 1.0 */
        float v;

        switch (type) {
        case WAVE_SIN:
            v = offset + amp * sinf(2.0f * (float)M_PI * t);
            break;
        case WAVE_SQUARE:
            v = (t < 0.5f) ? (offset + amp) : (offset - amp);
            break;
        case WAVE_TRI:
            v = (t < 0.5f)
                ? (offset - amp + 4.0f * amp * t)
                : (offset + 3.0f * amp - 4.0f * amp * t);
            break;
        case WAVE_SAW:
            v = offset - amp + 2.0f * amp * t;
            break;
        default:
            v = offset;
            break;
        }

        /* Clamp to 0 ~ VREF */
        if (v < 0.0f) v = 0.0f;
        if (v > DAC7311_VREF) v = DAC7311_VREF;

        s_wave_lut[i] = (uint16_t)((v / DAC7311_VREF) * 4095.0f + 0.5f);
    }
}

/* Timer callback: update DAC from LUT */
static void wave_timer_cb(void *parameter)
{
    (void)parameter;
    uint16_t raw = s_wave_lut[s_wave_idx];
    dac7311_set_raw(raw);
    s_wave_idx++;
    if (s_wave_idx >= WAVE_LUT_SIZE) {
        s_wave_idx = 0;
    }
}

/* Start waveform output */
static void wave_start(wave_type_t type, float freq, float amp, float offset)
{
    /* Clamp parameters */
    if (freq < 0.1f)    freq = 0.1f;
    if (freq > 1000.0f) freq = 1000.0f;
    if (amp < 0.0f)     amp = 0.0f;
    if (amp > DAC7311_VREF) amp = DAC7311_VREF;
    if (offset < 0.0f)  offset = 0.0f;
    if (offset > DAC7311_VREF) offset = DAC7311_VREF;
    if (offset + amp > DAC7311_VREF) amp = DAC7311_VREF - offset;
    if (offset - amp < 0.0f) amp = offset;

    /* Stop existing waveform first */
    if (s_wave_timer != RT_NULL) {
        rt_timer_stop(s_wave_timer);
        rt_timer_delete(s_wave_timer);
        s_wave_timer = RT_NULL;
    }

    /* Build LUT */
    s_wave_type   = type;
    s_wave_freq   = freq;
    s_wave_amp    = amp;
    s_wave_offset = offset;
    s_wave_idx    = 0;
    wave_build_lut(type, amp, offset);

    /* Timer period: T_ms = 1000 / (LUT_SIZE * freq) */
    rt_tick_t period_ms = (rt_tick_t)(1000.0f / ((float)WAVE_LUT_SIZE * freq));
    if (period_ms < 1) period_ms = 1;

    s_wave_timer = rt_timer_create("wave", wave_timer_cb,
                                    RT_NULL, period_ms,
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_wave_timer != RT_NULL) {
        rt_timer_start(s_wave_timer);
    } else {
        rt_kprintf("[WAVE] ERROR: Failed to create timer!\n");
    }
}

/* Stop waveform output */
static void wave_stop(void)
{
    if (s_wave_timer != RT_NULL) {
        rt_timer_stop(s_wave_timer);
        rt_timer_delete(s_wave_timer);
        s_wave_timer = RT_NULL;
    }
    s_wave_type = WAVE_NONE;
    s_wave_idx  = 0;
}

static const char* wave_type_name(wave_type_t t)
{
    switch (t) {
    case WAVE_SIN:    return "sin";
    case WAVE_SQUARE: return "square";
    case WAVE_TRI:    return "tri";
    case WAVE_SAW:    return "saw";
    default:          return "none";
    }
}

/* ============================================================================
 *  SPI Protocol Test Mode (repeat fixed frame for oscilloscope capture)
 * ===========================================================================*/
static rt_timer_t  s_test_timer  = RT_NULL;
static uint16_t    s_test_frame  = 0x8000;   /* default: 2.5V mid-scale */
static uint32_t    s_test_count = 0;

static void test_timer_cb(void *parameter)
{
    (void)parameter;
    dac7311_write_raw_frame(s_test_frame);
    s_test_count++;
}

static void test_start(uint16_t frame, uint32_t interval_ms)
{
    if (s_test_timer != RT_NULL) {
        rt_timer_stop(s_test_timer);
        rt_timer_delete(s_test_timer);
        s_test_timer = RT_NULL;
    }

    s_test_frame = frame;
    s_test_count = 0;

    s_test_timer = rt_timer_create("spitest", test_timer_cb,
                                    RT_NULL, interval_ms,
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_test_timer != RT_NULL) {
        rt_timer_start(s_test_timer);
    } else {
        rt_kprintf("[TEST] ERROR: Failed to create timer!\n");
    }
}

static void test_stop(void)
{
    if (s_test_timer != RT_NULL) {
        rt_timer_stop(s_test_timer);
        rt_timer_delete(s_test_timer);
        s_test_timer = RT_NULL;
    }
    s_test_count = 0;
}

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
  rt_kprintf("[main] Use shell command: dac <voltage|raw|pct|pd|wave> <value>\n");

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
        rt_kprintf("  dac wave <type> [freq] [amp] [offset]  Waveform output\n");
        rt_kprintf("  dac test [interval_ms] [hex_frame]  SPI protocol test\n");
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
    } else if (rt_strcmp(argv[1], "wave") == 0) {
        if (argc < 3) {
            rt_kprintf("Usage:\n");
            rt_kprintf("  dac wave sin    [freq] [amp] [offset]  Sine wave\n");
            rt_kprintf("  dac wave square [freq] [amp] [offset]  Square wave\n");
            rt_kprintf("  dac wave tri    [freq] [amp] [offset]  Triangle wave\n");
            rt_kprintf("  dac wave saw    [freq] [amp] [offset]  Sawtooth wave\n");
            rt_kprintf("  dac wave stop                           Stop waveform\n");
            rt_kprintf("  dac wave info                           Show status\n");
            rt_kprintf("  Defaults: freq=1Hz, amp=2.5V, offset=2.5V\n");
            return -RT_ERROR;
        }

        if (rt_strcmp(argv[2], "stop") == 0) {
            wave_stop();
            rt_kprintf("[WAVE] Stopped\n");
        } else if (rt_strcmp(argv[2], "info") == 0) {
            if (s_wave_type == WAVE_NONE) {
                rt_kprintf("[WAVE] Idle (no waveform active)\n");
            } else {
                rt_tick_t period_ms = (rt_tick_t)(1000.0f / ((float)WAVE_LUT_SIZE * s_wave_freq));
                rt_kprintf("[WAVE] Active: %s\n", wave_type_name(s_wave_type));
                rt_kprintf("  Freq:   %.2f Hz\n", s_wave_freq);
                rt_kprintf("  Amp:    %.3f V\n", s_wave_amp);
                rt_kprintf("  Offset: %.3f V\n", s_wave_offset);
                rt_kprintf("  LUT:    %d points, %d ms/sample\n", WAVE_LUT_SIZE, (int)period_ms);
                rt_kprintf("  Range:  %.3f ~ %.3f V\n",
                           s_wave_offset - s_wave_amp,
                           s_wave_offset + s_wave_amp);
            }
        } else {
            wave_type_t type = WAVE_NONE;
            if (rt_strcmp(argv[2], "sin") == 0)          type = WAVE_SIN;
            else if (rt_strcmp(argv[2], "square") == 0)  type = WAVE_SQUARE;
            else if (rt_strcmp(argv[2], "tri") == 0)     type = WAVE_TRI;
            else if (rt_strcmp(argv[2], "saw") == 0)     type = WAVE_SAW;
            else {
                rt_kprintf("[WAVE] Unknown type: %s\n", argv[2]);
                return -RT_ERROR;
            }

            float freq   = (argc > 3) ? atof(argv[3]) : 1.0f;
            float amp    = (argc > 4) ? atof(argv[4]) : 2.5f;
            float offset = (argc > 5) ? atof(argv[5]) : 2.5f;

            wave_start(type, freq, amp, offset);
            rt_kprintf("[WAVE] Started: %s, %.2fHz, amp=%.3fV, offset=%.3fV\n",
                       wave_type_name(type), freq, amp, offset);
            rt_kprintf("[WAVE] Range: %.3f ~ %.3f V\n",
                       offset - amp < 0.0f ? 0.0f : offset - amp,
                       offset + amp > DAC7311_VREF ? DAC7311_VREF : offset + amp);
        }
    } else if (rt_strcmp(argv[1], "test") == 0) {
        if (argc < 3) {
            rt_kprintf("Usage:\n");
            rt_kprintf("  dac test [interval_ms] [hex_frame]  Repeat SPI frame\n");
            rt_kprintf("  dac test stop                       Stop test mode\n");
            rt_kprintf("  dac test info                       Show status\n");
            rt_kprintf("  Defaults: interval=10ms, frame=0x8000 (2.5V)\n");
            rt_kprintf("\n  Probe: PB7=SYNC, PB8=SCLK, PB9=DIN\n");
            return -RT_ERROR;
        }

        if (rt_strcmp(argv[2], "stop") == 0) {
            test_stop();
            rt_kprintf("[TEST] Stopped (sent %d frames)\n", (int)s_test_count);
        } else if (rt_strcmp(argv[2], "info") == 0) {
            if (s_test_timer == RT_NULL) {
                rt_kprintf("[TEST] Idle\n");
            } else {
                rt_kprintf("[TEST] Active\n");
                rt_kprintf("  Frame:    0x%04X\n", s_test_frame);
                rt_kprintf("  Sent:     %d frames\n", (int)s_test_count);
                rt_kprintf("  Probe:    PB7=SYNC, PB8=SCLK, PB9=DIN\n");
                rt_kprintf("  Trigger:  Set scope to trigger on SYNC falling edge\n");
            }
        } else {
            uint32_t interval = (argc > 2) ? atoi(argv[2]) : 10;
            uint16_t frame    = (argc > 3) ? (uint16_t)strtol(argv[3], RT_NULL, 16) : 0x8000;
            if (interval < 1) interval = 1;
            if (interval > 10000) interval = 10000;

            test_start(frame, interval);
            rt_kprintf("[TEST] Started: frame=0x%04X, interval=%dms\n", frame, (int)interval);
            rt_kprintf("[TEST] Probe: PB7=SYNC, PB8=SCLK, PB9=DIN\n");
            rt_kprintf("[TEST] Scope trigger: SYNC falling edge\n");
            rt_kprintf("[TEST] Expected: 16 SCLK cycles per SYNC low pulse\n");
            rt_kprintf("[TEST] Frame bits: ");
            for (int b = 15; b >= 0; b--) {
                rt_kprintf("%d", (frame >> b) & 1);
                if (b == 14 || b == 12) rt_kprintf(" ");  /* visual separator */
            }
            rt_kprintf("\n");
            rt_kprintf("[TEST]          [X][X][PD1][PD0][D11..D0]\n");
        }
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
