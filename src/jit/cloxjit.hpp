#ifndef clox_jit_hpp
#define clox_jit_hpp

#include "asmjit/core.h"
#include "vm.h"
#include "jit.h"
#include <vector>
#include <cstdio>


class MyErrorHandler : public asmjit::ErrorHandler {
public:
  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    printf("AsmJit error: %s\n", message);
  }
};

class CloxJit {
  public:
    CloxJit();
    ~CloxJit();
    JittedFn jit(ObjClosure *closure);

  private:
    asmjit::FileLogger logger;
    MyErrorHandler myErrorHandler;

    asmjit::JitRuntime rt;

    asmjit::CodeHolder code;

    std::vector<JittedFn> jittedFns;
};

#endif

