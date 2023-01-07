#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "../clox.h"
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
  double pin = AS_NUMBER(args[0]);
  gpio_init((uint)pin); // TODO verify
  return NIL_VAL;
}

static Value getLedPinNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("getLedPin() takes exactly 0 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  #ifdef PICO_DEFAULT_LED_PIN
  return NUMBER_VAL(PICO_DEFAULT_LED_PIN);
  #else
  return NIL_VAL;
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
  double pin = AS_NUMBER(args[0]);
  gpio_set_dir((uint)pin, GPIO_OUT);
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
  double pin = AS_NUMBER(args[0]);
  gpio_put((uint)pin, 1);
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
  double pin = AS_NUMBER(args[0]);
  gpio_put((uint)pin, 0);
  return NIL_VAL;
}

bool registerModule_pico() {
  stdio_init_all();
  registerNativeMethod("sleep", sleepMsNative);
  registerNativeMethod("__init_pin", initPinNative);
  registerNativeMethod("__get_led_pin", getLedPinNative);
  registerNativeMethod("__pin_output", pinOutputNative);
  registerNativeMethod("__pin_on", pinOnNative);
  registerNativeMethod("__pin_off", pinOffNative);
  char *code = strndup(pico_module_bundle, pico_module_bundle_len);
  registerLoxCode(code);
  free(code);
  return true;
}

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