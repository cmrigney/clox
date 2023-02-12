#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "pico/stdlib.h"
#ifdef PICO_TARGET_STDIO_USB
#include "tusb.h"
#endif
#ifdef USE_PICO_W
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "clox-pico-w.h"
#endif
#include "../clox.h"
#include "machine.h"
#include "autogen/pico_lib.h"

static Value sleepMsNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("sleepMs() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("sleepMs() argument must be a number.");
    return NIL_VAL;
  }
  double ms = AS_NUMBER(args[0]);
  sleep_ms(ms);
  return NIL_VAL;
}

static Value initPinNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("initPin() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("initPin() argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL; // No init in this case
  }
  #endif
  gpio_init(pin);
  return NIL_VAL;
}

static Value getLedPinNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("getLedPin() takes exactly 0 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  #ifdef USE_PICO_W
  return NUMBER_VAL(CYW43_WL_GPIO_LED_PIN);
  #else
    #ifdef PICO_DEFAULT_LED_PIN
    return NUMBER_VAL(PICO_DEFAULT_LED_PIN);
    #else
    return NIL_VAL;
    #endif
  #endif
}

static Value pinOutputNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinOutput() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinOutput() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_set_dir(pin, GPIO_OUT);
  return NIL_VAL;
}

static Value pinInputNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinInput() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinInput() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_set_dir(pin, GPIO_IN);
  return NIL_VAL;
}

static Value pinReadNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinGet() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinGet() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL;
  }
  #endif
  return BOOL_VAL(gpio_get(pin));
}

static Value pinPullUpNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinPullUpNative() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinPullUpNative() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_pull_up(pin);
  return NIL_VAL;
}

static Value pinPullDownNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinPullUpNative() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinPullUpNative() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_pull_down(pin);
  return NIL_VAL;
}

static Value pinOnNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinOutput() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinOutput() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_set_function(pin, GPIO_FUNC_SIO);
  gpio_put(pin, 1);
  return NIL_VAL;
}

static Value pinOffNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("pinOutput() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("pinOutput() first argument must be a number.");
    return NIL_VAL;
  }
  uint pin = (uint)AS_NUMBER(args[0]);
  #ifdef USE_PICO_W
  if(pin == CYW43_WL_GPIO_LED_PIN) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    return NIL_VAL; // No dir change in this case
  }
  #endif
  gpio_set_function(pin, GPIO_FUNC_SIO);
  gpio_put(pin, 0);
  return NIL_VAL;
}

static Value isWNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("isW() takes exactly 0 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  #ifdef USE_PICO_W
  return BOOL_VAL(true);
  #else
  return BOOL_VAL(false);
  #endif
}

static inline void setInstanceField(ObjInstance *instance, const char *name, Value value) {
  ObjString *str = copyString(name, strlen(name));
  push(OBJ_VAL(str));
  tableSet(&instance->fields, str, value);
  pop();
}

extern char __StackLimit; /* Set by linker.  */
extern char end; /* Set by linker.  */
static Value getPicoStatsNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("getPicoStats() takes exactly 0 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  ObjInstance *instance = createObjectInstance();
  push(OBJ_VAL(instance));

  void *heapCurrent = sbrk(0);
  void *heapTop = &__StackLimit; // Maximum heap ptr: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_runtime/runtime.c
  ptrdiff_t heapUnusedSys = (uintptr_t)heapTop - (uintptr_t)heapCurrent;
  ptrdiff_t usedHeapSys = (uintptr_t)heapCurrent - (uintptr_t)&end;

  Obj *highestHeapObj = vm.objects;
  Obj *object = vm.objects;
  while(object != NULL) {
    if(object > highestHeapObj) {
      highestHeapObj = object;
    }
    object = object->next;
  }
  ptrdiff_t heapUnusedManaged = (uintptr_t)heapTop - (uintptr_t)highestHeapObj - objStructSize(highestHeapObj);
  
  void *ptr = malloc(1);
  ptrdiff_t heapUnusedUnmanaged = (uintptr_t)heapTop - (uintptr_t)ptr + 1;
  free(ptr);

  setInstanceField(instance, "sys_heap_used", NUMBER_VAL((double)usedHeapSys));
  setInstanceField(instance, "sys_heap_unused", NUMBER_VAL((double)heapUnusedSys));
  setInstanceField(instance, "sys_unmanaged_heap_unused_by_malloc", NUMBER_VAL((double)heapUnusedUnmanaged));
  setInstanceField(instance, "sys_managed_heap_unused", NUMBER_VAL((double)heapUnusedManaged));

  int var;
  ptr = &var;
  ptrdiff_t stackUnused = (uintptr_t)ptr - (uintptr_t)heapTop;
  setInstanceField(instance, "sys_stack_unused", NUMBER_VAL((double)stackUnused));

  return pop();
}

static Value exitNative(Value *receiver, int argCount, Value *args) {
  #ifdef USE_PICO_W
  cyw43_arch_deinit();
  #endif
  exit(0);
  return NIL_VAL;
}

bool registerModule_pico() {
  stdio_init_all();
  // sleep_ms(5000);
  #ifdef USE_PICO_W
  if (cyw43_arch_init()) {
    printf("WiFi init failed");
    return -1;
  }
  registerPicoWFunctions();
  #endif
  registerNativeMethod("sleep", sleepMsNative);
  registerNativeMethod("exit", exitNative);
  registerNativeMethod("isW", isWNative);
  registerNativeMethod("getPicoStats", getPicoStatsNative);
  registerNativeMethod("lowPowerSleep", powerSleepNative);

  registerNativeMethod("__init_pin", initPinNative);
  registerNativeMethod("__get_led_pin", getLedPinNative);
  registerNativeMethod("__pin_output", pinOutputNative);
  registerNativeMethod("__pin_input", pinInputNative);
  registerNativeMethod("__pin_read", pinReadNative);
  registerNativeMethod("__pin_pull_up", pinPullUpNative);
  registerNativeMethod("__pin_pull_down", pinPullDownNative);
  registerNativeMethod("__pin_on", pinOnNative);
  registerNativeMethod("__pin_off", pinOffNative);
  char *code = strndup(pico_module_bundle, pico_module_bundle_len);
  registerLoxCode(code);
  free(code);
  return true;
}

void unregisterModule_pico() {
  #ifdef USE_PICO_W
  cyw43_arch_deinit();
  #endif
}

#ifndef PICO_TARGET_STDIO_USB
bool tud_cdc_connected() {
  return true; // stub out for UART
}
#endif

static bool readline(char *buffer, size_t bufferLength) {
  char u, *p;
  for(p = buffer, u = getchar(); tud_cdc_connected() && u !='|' && u != EOF && p - buffer < bufferLength - 1; u = getchar()) {
    *p++ = u;
  }
  *p = 0;
  if(u == '|') {
    putchar('\n');
  }
  return tud_cdc_connected() && u != EOF;
}

void pico_repl() {
  char line[1024];
  interpret("var pico = systemImport(\"pico\");");
  for (;;) {
    while (!tud_cdc_connected()) { sleep_ms(100);  }

    printf("> ");

    if (!readline(line, sizeof(line))) {
      printf("\n");
      continue;
    }

    interpret(line);
  }
}