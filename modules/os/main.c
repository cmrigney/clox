#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "../clox.h"

static Value sleepNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("sleep() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("sleep() argument must be a number.");
    return NIL_VAL;
  }
  double ms = AS_NUMBER(args[0]);
  sleep(ms / (double)1000);
  return NIL_VAL;
}

bool registerModule_os() {
  registerNativeMethod("sleep", sleepNative);
  return true;
}