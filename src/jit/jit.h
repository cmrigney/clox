
#ifndef clox_jit_h
#define clox_jit_h

#include "common.h"
#include "object.h"
#include "vm.h"

typedef void (*JittedFn)(CallFrame *frame);

extern bool jitEnabled;

EXPORT JittedFn jitLoxClosure(ObjClosure *closure);

static inline void enableJit(bool enabled) {
  jitEnabled = enabled;
}

#endif
