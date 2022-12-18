#include <time.h>

#include "common.h"
#include "value.h"

Value clockNative(Value *receiver, int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}