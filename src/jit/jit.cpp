#include "asmjit/a64.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include <map>
#include "cloxjit.hpp"

CloxJit cloxJit;
bool jitEnabled;

JittedFn jitLoxClosure(ObjClosure *closure) {
  return cloxJit.jit(closure);
}