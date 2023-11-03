#include "asmjit/a64.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include <map>

using namespace asmjit;

typedef void (*Arity0Fn)();

static bool isFalsey(Value value) {
  // printf("Is falsy\n");
  // printValue(value);
  // printf("\n");
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static Value getGlobal(Table *globals, ObjString *name) {
  Value value;
  if (tableGet(globals, name, &value)) {
    // printf("Got global for %s\n", name->chars);
    // printValue(value);
    // printf("\n");
    return value;
  }

  // printf("No global for %s\n", name->chars);

  // TODO runtime error
  return NIL_VAL;
}

#ifdef DEBUG_TRACE_EXECUTION
static void printStack(Chunk *chunk, uint64_t offset) {
  printf("          ");
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    printf("[ ");
    printValue(*slot);
    printf(" ]");
  }
  printf("\n");
  disassembleInstruction(chunk, (int)offset);
}
#endif

static Value callHandler(Value callee, int argCount) {
  if(IS_OBJ(callee)) {
    if(OBJ_TYPE(callee) == OBJ_NATIVE) {
      NativeFn native = AS_NATIVE(callee);
      return native(NULL, argCount, vm.stackTop - argCount);
    }
  }
  return NIL_VAL; // TODO handle other cases
}

class MyErrorHandler : public ErrorHandler {
public:
  void handleError(Error err, const char* message, BaseEmitter* origin) override {
    printf("AsmJit error: %s\n", message);
  }
};

static std::map<int, Label> buildJumpTable(ObjFunction *function, a64::Compiler &cc)
{
  std::map<int, Label> jumpTable;
  for (int i = 0; i < function->chunk.count; i++)
  {
    #define SKIP_OP(skipOp, argBytes) \
      case skipOp: { \
        i += argBytes; \
        continue; \
      }

    uint8_t op = function->chunk.code[i];
    if (op == OP_JUMP || op == OP_JUMP_IF_FALSE)
    {
      uint16_t offset = (uint16_t)((function->chunk.code[i + 1] << 8) | function->chunk.code[i + 2]);
      i += 2;
      jumpTable[i + offset + 1] = cc.newLabel();
      continue;
    }
    if(op == OP_LOOP) {
      uint16_t offset = (uint16_t)((function->chunk.code[i + 1] << 8) | function->chunk.code[i + 2]);
      i += 2;
      jumpTable[i - offset + 1] = cc.newLabel();
      continue;
    }

    switch(op) {
      SKIP_OP(OP_CONSTANT, 1)
      SKIP_OP(OP_GET_LOCAL, 1)
      SKIP_OP(OP_SET_LOCAL, 1)
      SKIP_OP(OP_GET_GLOBAL, 1)
      SKIP_OP(OP_DEFINE_GLOBAL, 1)
      SKIP_OP(OP_SET_GLOBAL, 1)
      SKIP_OP(OP_GET_UPVALUE, 1)
      SKIP_OP(OP_SET_UPVALUE, 1)
      SKIP_OP(OP_GET_PROPERTY, 1)
      SKIP_OP(OP_SET_PROPERTY, 1)
      SKIP_OP(OP_GET_SUPER, 1)
      SKIP_OP(OP_SET_PROPERTY_SHADOWED, 1)
      SKIP_OP(OP_CALL, 1)
      SKIP_OP(OP_INVOKE, 2)
      SKIP_OP(OP_SUPER_INVOKE, 2)
      SKIP_OP(OP_CLASS, 1)
      SKIP_OP(OP_METHOD, 1)
      case OP_CLOSURE: { // closure needs to count args
        uint8_t constant = function->chunk.code[i + 1];
        ObjFunction *function = AS_FUNCTION(function->chunk.constants.values[constant]);
        i += 1;
        for (int j = 0; j < function->upvalueCount; j++)
        {
          // isLocal and index
          i += 2;
        }
        continue;
      }
      default:
        // Other ops don't offset with args
        continue;
    }

    #undef SKIP_OK
  }
  return jumpTable;
}

static void compileFunction(ObjClosure *closure)
{
  auto chunk = &closure->function->chunk;
  auto ip = chunk->code;

  FileLogger logger(stdout);
  MyErrorHandler myErrorHandler;

  JitRuntime rt;

  CodeHolder code;
  code.init(rt.environment(), rt.cpuFeatures());
  code.setLogger(&logger);
  code.setErrorHandler(&myErrorHandler);

  a64::Compiler cc(&code);

  auto jumpTable = buildJumpTable(closure->function, cc);

  FuncNode *funcNode;

  switch (closure->function->arity)
  {
  case 0:
    funcNode = cc.addFunc(FuncSignatureT<Value>());
    break;
  case 1:
    funcNode = cc.addFunc(FuncSignatureT<Value, Value>());
    break;
  case 2:
    funcNode = cc.addFunc(FuncSignatureT<Value, Value, Value>());
    break;
  case 3:
    funcNode = cc.addFunc(FuncSignatureT<Value, Value, Value, Value>());
    break;
  default:
    // TODO fix
    printf("Unsupported arity: %d\n", closure->function->arity);
    exit(1);
  }

  a64::Mem constants[chunk->constants.count];
  for (int i = 0; i < chunk->constants.count; i++)
  {
    constants[i] = cc.newUInt64Const(ConstPoolScope::kLocal, chunk->constants.values[i]);
  }

  a64::Gp ret = cc.newGpx();

  auto TRUE_VAL_MEM = cc.newUInt64Const(ConstPoolScope::kGlobal, TRUE_VAL);
  auto FALSE_VAL_MEM = cc.newUInt64Const(ConstPoolScope::kGlobal, FALSE_VAL);
  auto NIL_VAL_MEM = cc.newUInt64Const(ConstPoolScope::kGlobal, NIL_VAL);

  Value **stackTopPtr = &vm.stackTop;
  auto stackTopPtrReg = cc.newUIntPtr();
  cc.mov(stackTopPtrReg, (uint64_t)stackTopPtr);

  Value *baseStackPtr = vm.stack; // TODO should be beginning of function stack
  auto baseStackPtrReg = cc.newUIntPtr();
  cc.mov(baseStackPtrReg, (uint64_t)baseStackPtr);

  int registerStackTop = 0;
  a64::Gp registerStack[256];

#define FLUSH_REGISTER_CACHE() \
  do { \
    if(registerStackTop > 0) { \
      auto derefStackTopPtrReg = cc.newGpx(); \
      cc.ldr(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
      for(int i = 0; i < registerStackTop; i++) { \
        cc.str(registerStack[i], a64::Mem(derefStackTopPtrReg)); \
        cc.add(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value)); \
      } \
      cc.str(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
      registerStackTop = 0; \
    } \
  } while (0)

/*
// Also a possible way to store multiple faster
if(registerStackTop >= 2) { \
  for(int i = 0; i < registerStackTop; i += 2) { \
    cc.stp(registerStack[i], registerStack[i + 1], a64::Mem(derefStackTopPtrReg)); \
    cc.add(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value) * 2); \
  } \
} \
if(registerStackTop % 2 == 1) { \
  cc.str(registerStack[registerStackTop - 1], a64::Mem(derefStackTopPtrReg)); \
  cc.add(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value)); \
} \
*/

#define PUSH_REG(value) \
  do { \
    if(registerStackTop >= 256) { \
      printf("Stack overflow\n"); \
      exit(1); \
    } \
    registerStack[registerStackTop++] = value; \
  } while (0)
    // auto derefStackTopPtrReg = cc.newGpx(); \
    // cc.ldr(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
    // cc.str(value, a64::Mem(derefStackTopPtrReg)); \
    // cc.add(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value)); \
    // cc.str(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \

#define PUSH_MEM(value) \
  do { \
    PUSH_REG(cc.newGpx()); \
    cc.ldr(registerStack[registerStackTop - 1], value); \
  } while (0)
    // auto valueReg = cc.newGpx(); \
    // auto derefStackTopPtrReg = cc.newGpx(); \
    // cc.ldr(valueReg, value); \
    // cc.ldr(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
    // cc.str(valueReg, a64::Mem(derefStackTopPtrReg)); \
    // cc.add(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value)); \
    // cc.str(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \

#define SEEK(OUT, PTR, n) \
  FLUSH_REGISTER_CACHE(); \
  auto OUT = cc.newGpx(); \
  auto PTR = cc.newUIntPtr(); \
  do { \
    cc.mov(PTR, baseStackPtr); \
    if(n > 0) { \
      cc.add(PTR, PTR, sizeof(Value) * (n)); \
    } \
    cc.ldr(OUT, a64::Mem(PTR)); \
  } while(0)

#define PEEK(OUT, n) \
  a64::Gp OUT; \
  do { \
    if(n < registerStackTop) { \
      OUT = registerStack[registerStackTop - n - 1]; \
    } \
    else { \
      OUT = cc.newGpx(); \
      auto ptrReg = cc.newUIntPtr(); \
      auto derefStackTopPtrReg = cc.newGpx(); \
      cc.ldr(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
      cc.sub(ptrReg, derefStackTopPtrReg, sizeof(Value) * (n + 1)); \
      cc.ldr(OUT, a64::Mem(ptrReg)); \
    } \
  } while (0)

#define POP() \
  do { \
    if(registerStackTop < 0) { \
      printf("Stack underflow\n"); \
      exit(1); \
    } \
    if(registerStackTop > 0) { \
      registerStackTop--; \
    } \
    else { \
      auto derefStackTopPtrReg = cc.newGpx(); \
      cc.ldr(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
      cc.sub(derefStackTopPtrReg, derefStackTopPtrReg, sizeof(Value)); \
      cc.str(derefStackTopPtrReg, a64::Mem(stackTopPtrReg)); \
    } \
  } while (0)

#define READ_BYTE() (*ip++)

#define READ_SHORT() \
    (ip += 2, \
    (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_CONSTANT() \
    (constants[READ_BYTE()])
  
  bool done = false;

  for (;;)
  {
    if(done) {
      break;
    }

    if(jumpTable.find(ip - chunk->code) != jumpTable.end()) {
      FLUSH_REGISTER_CACHE(); // Anything that jumps to this needs a fresh start on registers
      cc.bind(jumpTable[ip - chunk->code]);
    }

    #ifdef DEBUG_TRACE_EXECUTION
    // print stack
    FLUSH_REGISTER_CACHE();
    auto printStackReg = cc.newGpx();
    cc.mov(printStackReg, (uint64_t)&printStack);
    InvokeNode* call_printStack;
    cc.invoke(&call_printStack, printStackReg, FuncSignatureT<void, Chunk*, uint64_t>());
    call_printStack->setArg(0, (uint64_t)chunk);
    call_printStack->setArg(1, (uint64_t)(ip - chunk->code));
    #endif

    auto op = *ip++;

    switch (op)
    {
    case OP_CONSTANT: {
      auto constant = READ_CONSTANT();
      PUSH_MEM(constant);
      break;
    }
    case OP_GET_LOCAL: {
      auto slot = READ_BYTE();
      SEEK(slotReg, ptrReg, slot);
      PUSH_REG(slotReg);
      break;
    }
    case OP_SET_LOCAL: {
      auto slot = READ_BYTE();
      SEEK(slotReg, ptrReg, slot);
      PEEK(valueReg, 0);
      // Don't pop
      cc.str(valueReg, a64::Mem(ptrReg));
      break;
    }
    case OP_GET_GLOBAL: {
      FLUSH_REGISTER_CACHE();
      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();
      
      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, ~(SIGN_BIT | QNAN));
      
      auto getGlobalReg = cc.newGpx();
      cc.mov(getGlobalReg, (uint64_t)&getGlobal);
      InvokeNode* call_getGlobal;
      cc.invoke(&call_getGlobal, getGlobalReg, FuncSignatureT<Value, Table*, ObjString*>());
      call_getGlobal->setArg(0, &vm.globals);
      call_getGlobal->setArg(1, constantReg);
      auto resultReg = cc.newGpx();
      call_getGlobal->setRet(0, resultReg);

      PUSH_REG(resultReg);
      break;
    }
    case OP_GREATER: {
      // TODO verify a number
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      auto vb = cc.newVecD();
      // Number conversion is bit for bit
      cc.mov(va.d(0), a);
      cc.mov(vb.d(0), b);

      auto trueReg = cc.newGpx();
      auto falseReg = cc.newGpx();
      cc.ldr(trueReg, TRUE_VAL_MEM);
      cc.ldr(falseReg, FALSE_VAL_MEM);
      cc.fcmpe(va, vb);
      cc.csel(a, trueReg, falseReg, a64::CondCode::kGT);

      PUSH_REG(a);
      break;
    }
    case OP_NOT: {
      PEEK(a, 0);
      POP();

      auto isFalseyReg = cc.newGpx();
      cc.mov(isFalseyReg, (uint64_t)&isFalsey);
      InvokeNode* call_isFalsey;
      cc.invoke(&call_isFalsey, isFalseyReg, FuncSignatureT<bool, Value>());
      call_isFalsey->setArg(0, a);
      auto resultReg = cc.newGpw();
      call_isFalsey->setRet(0, resultReg);
      
      auto trueReg = cc.newGpx();
      auto falseReg = cc.newGpx();
      cc.ldr(trueReg, TRUE_VAL_MEM);
      cc.ldr(falseReg, FALSE_VAL_MEM);
      cc.cmp(resultReg, true); // is true that it's falsy
      cc.csel(a, trueReg, falseReg, a64::CondCode::kEQ);
      PUSH_REG(a);
      break;
    }
    case OP_CALL: {
      FLUSH_REGISTER_CACHE();
      // TODO
      int argCount = READ_BYTE();
      // Pointer to ObjClosure* is in top - 1 - argCount
      PEEK(calleeReg, argCount);

      auto callHandlerReg = cc.newGpx();
      cc.mov(callHandlerReg, (uint64_t)&callHandler);
      InvokeNode* call_callHandler;

      cc.invoke(&call_callHandler, callHandlerReg, FuncSignatureT<Value, Value, int>());
      call_callHandler->setArg(0, calleeReg);
      call_callHandler->setArg(1, argCount);
      auto resultReg = cc.newGpx();
      call_callHandler->setRet(0, resultReg);

      // TODO this could be make more efficient with a POP(argCount + 1)
      for(int i = 0; i < argCount; i++) {
        POP(); // pop args
      }
      POP(); // pop fn

      PUSH_REG(resultReg);
      break;
    }
    case OP_JUMP: {
      FLUSH_REGISTER_CACHE();
      auto offset = READ_SHORT();
      auto jumpLabel = jumpTable[ip - chunk->code + offset];
      cc.b(jumpLabel); // TODO verify this won't mess up registers
      break;
    }
    case OP_JUMP_IF_FALSE: {
      FLUSH_REGISTER_CACHE();
      auto offset = READ_SHORT();
      auto jumpLabel = jumpTable[ip - chunk->code + offset];

      PEEK(a, 0);
      // Don't pop
      
      auto isFalseyReg = cc.newGpx();
      cc.mov(isFalseyReg, (uint64_t)&isFalsey);
      InvokeNode* call_isFalsey;
      cc.invoke(&call_isFalsey, isFalseyReg, FuncSignatureT<bool, Value>());
      call_isFalsey->setArg(0, a);
      auto resultReg = cc.newGpw();
      call_isFalsey->setRet(0, resultReg);

      // if not 0 (ie true) branch
      cc.cbnz(resultReg, jumpLabel);
      break;
    }
    case OP_LOOP: {
      FLUSH_REGISTER_CACHE();
      auto offset = READ_SHORT();
      auto jumpLabel = jumpTable[ip - chunk->code - offset];
      cc.b(jumpLabel);
      break;
    }
    case OP_FALSE: {
      PUSH_MEM(FALSE_VAL_MEM);
      break;
    }
    case OP_TRUE: {
      PUSH_MEM(TRUE_VAL_MEM);
      break;
    }
    case OP_NIL: {
      PUSH_MEM(NIL_VAL_MEM);
      break;
    }
    case OP_POP: {
      POP();
      break;
    }
    case OP_ADD: {
      // TODO we can't assume numbers
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      auto vb = cc.newVecD();
      cc.mov(va.d(0), a);
      cc.mov(vb.d(0), b);

      cc.fadd(va, va, vb);
      cc.mov(a, va.d(0));

      PUSH_REG(a);
      break;
    }
    case OP_SUBTRACT: {
      // TODO we can't assume numbers
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      auto vb = cc.newVecD();
      cc.mov(va.d(0), a);
      cc.mov(vb.d(0), b);

      cc.fsub(va, va, vb);
      cc.mov(a, va.d(0));

      PUSH_REG(a);
      break;
    }
    case OP_MULTIPLY: {
      // TODO we can't assume numbers
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      auto vb = cc.newVecD();
      cc.mov(va.d(0), a);
      cc.mov(vb.d(0), b);

      cc.fmul(va, va, vb);
      cc.mov(a, va.d(0));

      PUSH_REG(a);
      break;
    }
    case OP_DIVIDE: {
      // TODO we can't assume numbers
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      auto vb = cc.newVecD();
      cc.mov(va.d(0), a);
      cc.mov(vb.d(0), b);

      cc.fdiv(va, va, vb);
      cc.mov(a, va.d(0));

      PUSH_REG(a);
      break;
    }
    case OP_RETURN: {
      FLUSH_REGISTER_CACHE();

      PEEK(resultReg, 0);
      POP();

      // Extra pop if root function
      POP();
      done = true;
      break;
    }
    default:
      printf("Unsupported opcode: %d\n", op);
      exit(1);
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef PUSH_SETUP
#undef PUSH_MEM
#undef PUSH_REG
#undef PEEK
#undef POP

  cc.ret();
  cc.endFunc();
  cc.finalize();

  Arity0Fn fn;
  Error err = rt.add(&fn, &code);
  if (err)
  {
    printf("Error: %s\n", DebugUtils::errorAsString(err));
    return;
  }

  fn();

  rt.release(fn);
}

/*
== <script> ==
0000    2 OP_FALSE
0001    | OP_NOT
0002    3 OP_GET_GLOBAL       0 'logln'
0004    | OP_GET_LOCAL        1
0006    | OP_CALL             1
0008    | OP_POP
0009    4 OP_GET_GLOBAL       1 'logln'
0011    | OP_TRUE
0012    | OP_NOT
0013    | OP_CALL             1
0015    | OP_POP
0016    5 OP_POP
0017    | OP_NIL
0018    | OP_RETURN
*/

/*
0000    2 OP_CONSTANT         0 '0'
0002    3 OP_CONSTANT         1 '0'
0004    | OP_GET_LOCAL        2
0006    | OP_CONSTANT         2 '1e+08'
0008    | OP_GREATER
0009    | OP_NOT
0010    | OP_JUMP_IF_FALSE   10 -> 39
0013    | OP_POP
0014    | OP_JUMP            14 -> 28
0017    | OP_GET_LOCAL        2
0019    | OP_CONSTANT         3 '1'
0021    | OP_ADD
0022    | OP_SET_LOCAL        2
0024    | OP_POP
0025    | OP_LOOP            25 -> 4
0028    4 OP_GET_LOCAL        1
0030    | OP_GET_LOCAL        2
0032    | OP_ADD
0033    | OP_SET_LOCAL        1
0035    | OP_POP
0036    5 OP_LOOP            36 -> 17
0039    | OP_POP
0040    | OP_POP
0041    7 OP_GET_GLOBAL       4 'logln'
0043    | OP_GET_LOCAL        1
0045    | OP_CALL             1
0047    | OP_POP
0048    8 OP_POP
0049    | OP_NIL
0050    | OP_RETURN
*/

extern "C" InterpretResult compileAndRun(const char *source)
{
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));

  compileFunction(closure);
  return INTERPRET_OK;
}
