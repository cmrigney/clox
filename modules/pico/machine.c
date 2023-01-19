#include "../clox.h"

#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/sync.h"
#include "hardware/structs/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/syscfg.h"
#include "hardware/watchdog.h"
#include "hardware/xosc.h"
#include "pico/stdlib.h"


Value powerSleepNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("powerSleep() takes exactly 1 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("powerSleep() argument must be a number.");
    return NIL_VAL;
  }
  int delay_ms = (int)AS_NUMBER(args[0]);
  if(delay_ms <= 1) {
    sleep_ms(delay_ms); // sleep too small, use standard delay
    return NIL_VAL;
  }

  bool can_use_timer = delay_ms < (1ULL << 32) / 1000;
  if(!can_use_timer) {
    // runtimeError("powerSleep() argument too large.");
    return NIL_VAL;
  }

  // Logic borrowed from https://github.com/micropython/micropython/blob/master/ports/rp2/modmachine.c
  // Assumes we're using poll mode for CYW43
  const uint32_t xosc_hz = XOSC_MHZ * 1000000;

  uint32_t my_interrupts = save_and_disable_interrupts();

  // Disable USB and ADC clocks.
  clock_stop(clk_usb);
  clock_stop(clk_adc);

  // CLK_REF = XOSC
  clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, xosc_hz, xosc_hz);

  // CLK_SYS = CLK_REF
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0, xosc_hz, xosc_hz);

  // CLK_RTC = XOSC / 256
  clock_configure(clk_rtc, 0, CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, xosc_hz, xosc_hz / 256);

  // CLK_PERI = CLK_SYS
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, xosc_hz, xosc_hz);

  // Disable PLLs.
  pll_deinit(pll_sys);
  pll_deinit(pll_usb);

  // Disable ROSC.
  rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_DISABLE << ROSC_CTRL_ENABLE_LSB;

  uint32_t sleep_en0 = clocks_hw->sleep_en0;
  uint32_t sleep_en1 = clocks_hw->sleep_en1;
  clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS;
  // Use timer alarm to wake.
  clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS;
  timer_hw->alarm[3] = timer_hw->timerawl + delay_ms * 1000;
  scb_hw->scr |= M0PLUS_SCR_SLEEPDEEP_BITS;
  __wfi();
  scb_hw->scr &= ~M0PLUS_SCR_SLEEPDEEP_BITS;
  clocks_hw->sleep_en0 = sleep_en0;
  clocks_hw->sleep_en1 = sleep_en1;

  // Enable ROSC.
  rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;

  // Bring back all clocks.
  clocks_init();
  restore_interrupts(my_interrupts);

  return NIL_VAL;
}