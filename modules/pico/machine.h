#ifndef PICO_MACHINE_H
#define PICO_MACHINE_H

#include "../clox.h"

Value powerSleepNative(Value *receiver, int argCount, Value *args);
Value rebootOnRuntimeErrorNative(Value *receiver, int argCount, Value *args);
Value enableWatchdogNative(Value *receiver, int argCount, Value *args);
Value disableWatchdogNative(Value *receiver, int argCount, Value *args);
Value updateWatchdogNative(Value *receiver, int argCount, Value *args);

extern bool resetOnExit;

#endif