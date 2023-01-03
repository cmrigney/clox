#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "../clox.h"

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

bool registerModule_pico() {
  stdio_init_all();
  registerNativeMethod("sleep", sleepMsNative);
  return true;
}