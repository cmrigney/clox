#include "asmjit/a64.h"
#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "cloxjit.hpp"
#include <map>
#include <cstdio>

using namespace asmjit;

static bool isFalsey(Value value) {
  // printf("Is falsy\n");
  // printValue(value);
  // printf("\n");
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static Value getGlobal(Table *globals, ObjString *name) {
  Value value;
  if (tableGet(globals, name, &value)) {
    return value;
  }

  // TODO runtime error
  return NIL_VAL;
}

static void setGlobal(Table *globals, ObjString *name, Value value) {
  if(tableSet(globals, name, value)) {
    tableDelete(&vm.globals, name); 
    // TODO runtime error
    exit(1);
  }
}

static void defineGlobal(Table *globals, ObjString *name, Value value) {
  tableSet(globals, name, value);
}

static void debugExecOp(int op) {
  printf("Executing op: %d\n", op);
  fflush(stdout);
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

CloxJit::CloxJit() {
  logger.setFile(stdout);
}

CloxJit::~CloxJit() {
  for (auto it = jittedFns.begin(); it != jittedFns.end(); ++it) {
    rt.release(*it);
  }
}

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
        ObjFunction *fn = AS_FUNCTION(function->chunk.constants.values[constant]);
        i += 1;
        for (int j = 0; j < fn->upvalueCount; j++)
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

    #undef SKIP_OP
  }
  return jumpTable;
}

JittedFn CloxJit::jit(ObjClosure *closure) {
  CodeHolder code;

  code.init(rt.environment(), rt.cpuFeatures());
  if(getenv("CLOX_LOG_JIT")) {
    code.setLogger(&logger);
  }
  code.setErrorHandler(&myErrorHandler);

  auto chunk = &closure->function->chunk;
  auto ip = chunk->code;

  a64::Compiler cc(&code);

  auto jumpTable = buildJumpTable(closure->function, cc);

  FuncNode *funcNode = cc.addFunc(FuncSignatureT<InterpretResult, CallFrame*>());
  auto callFrameReg = cc.newGpx();
  funcNode->setArg(0, callFrameReg);

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

  auto slotBasePtr = cc.newUIntPtr();
  cc.add(slotBasePtr, callFrameReg, offsetof(CallFrame, slots));
  cc.ldr(slotBasePtr, a64::Mem(slotBasePtr));

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

#define PUSH_MEM(value) \
  do { \
    PUSH_REG(cc.newGpx()); \
    cc.ldr(registerStack[registerStackTop - 1], value); \
  } while (0)

#define SEEK(OUT, PTR, n) \
  FLUSH_REGISTER_CACHE(); \
  auto OUT = cc.newGpx(); \
  auto PTR = cc.newUIntPtr(); \
  do { \
    if(n > 0) { \
      cc.add(PTR, slotBasePtr, sizeof(Value) * n); \
    } \
    else { \
      cc.mov(PTR, slotBasePtr); \
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

#define PEEK_COMPILE_CONSTANT() \
    (chunk->constants.values[*ip])
  
  for (;;)
  {
    // After all code is compiled, break
    if(ip - chunk->code >= chunk->count) {
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

    // #define PRINT_OP_CODES

    #ifdef PRINT_OP_CODES
    printf("Jitting: %d\n", *ip);
    auto debugExecOpReg = cc.newGpx();
    cc.mov(debugExecOpReg, (uint64_t)&debugExecOp);
    InvokeNode* call_debugExecOp;
    cc.invoke(&call_debugExecOp, debugExecOpReg, FuncSignatureT<void, int>());
    call_debugExecOp->setArg(0, (int)*ip);
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
    case OP_SET_GLOBAL: {
      FLUSH_REGISTER_CACHE();
      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();

      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, ~(SIGN_BIT | QNAN));

      PEEK(valueReg, 0);

      auto setGlobalReg = cc.newGpx();
      cc.mov(setGlobalReg, (uint64_t)&setGlobal);
      InvokeNode* call_setGlobal;
      cc.invoke(&call_setGlobal, setGlobalReg, FuncSignatureT<void, Table*, ObjString*, Value>());
      call_setGlobal->setArg(0, &vm.globals);
      call_setGlobal->setArg(1, constantReg);
      call_setGlobal->setArg(2, valueReg);

      break;
    }
    case OP_DEFINE_GLOBAL: {
      FLUSH_REGISTER_CACHE();
      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();

      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, ~(SIGN_BIT | QNAN));

      PEEK(valueReg, 0);

      auto defineGlobalReg = cc.newGpx();
      cc.mov(defineGlobalReg, (uint64_t)&defineGlobal);
      InvokeNode* call_defineGlobal;
      cc.invoke(&call_defineGlobal, defineGlobalReg, FuncSignatureT<void, Table*, ObjString*, Value>());
      call_defineGlobal->setArg(0, &vm.globals);
      call_defineGlobal->setArg(1, constantReg);
      call_defineGlobal->setArg(2, valueReg);

      POP();
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
    case OP_LESS: {
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
      cc.csel(a, trueReg, falseReg, a64::CondCode::kLT);

      PUSH_REG(a);
      break;
    }
    case OP_EQUAL: {
      PEEK(b, 0);
      POP();
      PEEK(a, 0);
      POP();
      auto trueReg = cc.newGpx();
      auto falseReg = cc.newGpx();
      cc.ldr(trueReg, TRUE_VAL_MEM);
      cc.ldr(falseReg, FALSE_VAL_MEM);
      cc.cmp(a, b);
      cc.csel(a, trueReg, falseReg, a64::CondCode::kEQ);
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
      cc.mov(callHandlerReg, (uint64_t)&callValue);
      InvokeNode* call_callHandler;

      cc.invoke(&call_callHandler, callHandlerReg, FuncSignatureT<bool, Value, int>());
      call_callHandler->setArg(0, calleeReg);
      call_callHandler->setArg(1, argCount);
      auto resultReg = cc.newGpw();
      call_callHandler->setRet(0, resultReg);

      // TODO return runtime error if false result
      break;
    }
    case OP_GET_PROPERTY: {
      // TODO add support for array and string native methods
      // TODO verify is instance

      auto constant = READ_CONSTANT();
      auto keyReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(keyReg, constant);
      // AS_OBJ
      cc.and_(keyReg, keyReg, qnanBits);

      PEEK(instanceReg, 0);
      // AS_OBJ
      cc.and_(instanceReg, instanceReg, qnanBits);

      auto fieldsPtrReg = cc.newGpx();
      cc.add(fieldsPtrReg, instanceReg, offsetof(ObjInstance, fields));

      auto stackMem = cc.newStack(sizeof(Value), 8);
      auto tableGetNotFoundLabel = cc.newLabel();
      auto endLabel = cc.newLabel();
      auto valuePtrReg = cc.newGpx();
      auto error = cc.loadAddressOf(valuePtrReg, stackMem);
      if(error) {
        printf("Error: %s\n", DebugUtils::errorAsString(error));
        exit(1);
      }
      
      auto resultReg = cc.newGpw();
      auto tableGetReg = cc.newGpx();
      cc.mov(tableGetReg, (uint64_t)&tableGet);
      InvokeNode *call_tableGet;
      cc.invoke(&call_tableGet, tableGetReg, FuncSignatureT<bool, Table*, ObjString*, Value*>());
      call_tableGet->setArg(0, fieldsPtrReg);
      call_tableGet->setArg(1, keyReg);
      call_tableGet->setArg(2, valuePtrReg);
      call_tableGet->setRet(0, resultReg);

      FLUSH_REGISTER_CACHE(); // for garbage collector

      cc.cbz(resultReg, tableGetNotFoundLabel);

      auto valueReg = cc.newGpx();
      cc.ldr(valueReg, stackMem);
      POP();
      PUSH_REG(valueReg);
      cc.b(endLabel);

      cc.bind(tableGetNotFoundLabel);

      auto klassReg = cc.newGpx();
      cc.ldr(klassReg, a64::Mem(instanceReg, offsetof(ObjInstance, klass)));

      auto bindMethodReg = cc.newGpx();
      cc.mov(bindMethodReg, (uint64_t)&bindMethod);
      InvokeNode *call_bindMethod;
      cc.invoke(&call_bindMethod, bindMethodReg, FuncSignatureT<bool, ObjClass*, ObjString*>());
      call_bindMethod->setArg(0, klassReg);
      call_tableGet->setArg(1, keyReg);
      call_bindMethod->setRet(0, resultReg);

      // TODO runtime error if result is false
      cc.bind(endLabel);
      break;
    }
    case OP_SET_PROPERTY: {
      // TODO verify is instance

      // For some reason the flush has to come first but idk why. Otherwise it seg faults.
      FLUSH_REGISTER_CACHE(); // for garbage collector

      auto constant = READ_CONSTANT();
      auto keyReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(keyReg, constant);
      // AS_OBJ
      cc.and_(keyReg, keyReg, qnanBits);

      int opCode = *(ip - 1);
      PEEK(instanceReg, 1);
      PEEK(valueReg, 0);
      // AS_OBJ
      cc.and_(instanceReg, instanceReg, qnanBits);

      auto fieldsPtrReg = cc.newGpx();
      cc.add(fieldsPtrReg, instanceReg, offsetof(ObjInstance, fields));

      auto tableSetReg = cc.newGpx();
      cc.mov(tableSetReg, (uint64_t)&tableSet);
      InvokeNode *call_tableSet;
      cc.invoke(&call_tableSet, tableSetReg, FuncSignatureT<bool, Table*, ObjString*, Value>());
      call_tableSet->setArg(0, fieldsPtrReg);
      call_tableSet->setArg(1, keyReg);
      call_tableSet->setArg(2, valueReg);

      POP();
      POP();

      if(opCode == OP_SET_PROPERTY_SHADOWED) {
        // OBJ_VAL
        cc.orn(instanceReg, instanceReg, qnanBits);
        PUSH_REG(instanceReg);
      }
      else {
        PUSH_REG(valueReg);
      }

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
    case OP_NEGATE: {
      // TODO we can't assume numbers
      PEEK(a, 0);
      POP();
      auto va = cc.newVecD();
      cc.mov(va.d(0), a);
      cc.fneg(va, va);
      cc.mov(a, va.d(0));

      PUSH_REG(a);
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(PEEK_COMPILE_CONSTANT());

      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();
      auto functionReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(functionReg, constantReg, qnanBits);

      // Create new closure and push it
      auto newClosureReg = cc.newGpx();
      cc.mov(newClosureReg, (uint64_t)&newClosure);
      InvokeNode* call_newClosure;
      cc.invoke(&call_newClosure, newClosureReg, FuncSignatureT<ObjClosure*, ObjFunction*>());
      call_newClosure->setArg(0, functionReg);
      auto closureReg = cc.newGpx();
      call_newClosure->setRet(0, closureReg);

      auto closureObjReg = cc.newGpx();
      // OBJ_VAL
      // Because orr doesn't work for some reason
      cc.orn(closureObjReg, closureReg, qnanBits);
      PUSH_REG(closureObjReg);
      FLUSH_REGISTER_CACHE();

      for(int i = 0; i < function->upvalueCount; i++) {
        auto isLocal = READ_BYTE();
        auto index = READ_BYTE();
        auto isLocalReg = cc.newGpx();
        auto indexReg = cc.newGpx();
        cc.mov(isLocalReg, isLocal);

        auto upvalueReg = cc.newGpx();
        auto closureUpvaluePtrReg = cc.newGpx();
        cc.ldr(closureUpvaluePtrReg, a64::Mem(closureReg, offsetof(ObjClosure, upvalues)));

        auto elseBranch = cc.newLabel();
        auto endLabel = cc.newLabel();

        cc.cbz(isLocalReg, elseBranch);

        // add index * 8 to slotBasePtr
        cc.add(indexReg, slotBasePtr, index, 3);

        auto captureUpvalueReg = cc.newGpx();
        cc.mov(captureUpvalueReg, (uint64_t)&captureUpvalue);
        InvokeNode* call_captureUpvalue;
        cc.invoke(&call_captureUpvalue, captureUpvalueReg, FuncSignatureT<ObjUpvalue*, Value*>());
        call_captureUpvalue->setArg(0, indexReg);
        call_captureUpvalue->setRet(0, upvalueReg);

        // closure->upvalues[i] = upvalue
        cc.str(upvalueReg, a64::Mem(closureUpvaluePtrReg, i * sizeof(ObjUpvalue*)));

        cc.b(endLabel);

        cc.bind(elseBranch);
        // closure->upvalues[i] = frame->closure->upvalues[index]
        auto currentClosureUpvaluePtr = closure->upvalues; // Closure of current frame, safe at compile time because it won't change
        auto currentClosureUpvaluePtrReg = cc.newUIntPtr();
        cc.mov(currentClosureUpvaluePtrReg, (uint64_t)currentClosureUpvaluePtr);
        cc.ldr(upvalueReg, a64::Mem(currentClosureUpvaluePtrReg, index * sizeof(ObjUpvalue*)));
        cc.str(upvalueReg, a64::Mem(closureUpvaluePtrReg, i * sizeof(ObjUpvalue*)));

        cc.bind(endLabel);
      }

      break;
    }
    case OP_CLASS: {
      FLUSH_REGISTER_CACHE(); // for garbage collection
      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, qnanBits);

      auto newClassReg = cc.newGpx();
      cc.mov(newClassReg, (uint64_t)&newClass);
      InvokeNode* class_newClass;
      cc.invoke(&class_newClass, newClassReg, FuncSignatureT<ObjClass*, ObjString*>());
      class_newClass->setArg(0, constantReg);
      auto classReg = cc.newGpx();
      class_newClass->setRet(0, classReg);

      // OBJ_VAL
      // Because orr doesn't work for some reason
      cc.orn(classReg, classReg, qnanBits);

      PUSH_REG(classReg);
      break;
    }
    case OP_METHOD: {
      FLUSH_REGISTER_CACHE(); // for garbage collection

      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, qnanBits);

      auto methodReg = cc.newGpx();
      cc.mov(methodReg, (uint64_t)&defineMethod);
      InvokeNode* call_defineMethod;
      cc.invoke(&call_defineMethod, methodReg, FuncSignatureT<void, ObjString*>());
      call_defineMethod->setArg(0, constantReg);

      break;
    }
    case OP_INVOKE: {
      FLUSH_REGISTER_CACHE(); // for garbage collection

      auto constant = READ_CONSTANT();
      auto constantReg = cc.newGpx();
      auto qnanBits = cc.newGpx();
      cc.mov(qnanBits, ~(SIGN_BIT | QNAN));
      cc.ldr(constantReg, constant);
      // AS_OBJ
      cc.and_(constantReg, constantReg, qnanBits);

      int argCount = READ_BYTE();
      auto argCountReg = cc.newGpw();
      cc.mov(argCountReg, argCount);

      auto invokeReg = cc.newGpx();
      cc.mov(invokeReg, (uint64_t)&invoke);
      InvokeNode* call_invoke;
      cc.invoke(&call_invoke, invokeReg, FuncSignatureT<bool, ObjString*, int>());
      call_invoke->setArg(0, constantReg);
      call_invoke->setArg(1, argCountReg);
      auto resultReg = cc.newGpw();
      call_invoke->setRet(0, resultReg);

      // TODO return runtime error if false result
      break;
    }
    case OP_PRINT: {
      PEEK(a, 0);
      POP();

      auto printValueReg = cc.newGpx();
      cc.mov(printValueReg, (uint64_t)&printValue);
      InvokeNode* call_printValue;
      cc.invoke(&call_printValue, printValueReg, FuncSignatureT<void, Value>());
      call_printValue->setArg(0, a);

      auto printfReg = cc.newGpx();
      cc.mov(printfReg, (uint64_t)&printf);
      InvokeNode* call_printf;
      cc.invoke(&call_printf, printfReg, FuncSignatureT<int, const char*>());
      call_printf->setArg(0, (uint64_t)"\n");

      break;
    }
    case OP_RETURN: {

      // closeUpvalues(frame->slots);
      auto callUpvaluesReg = cc.newGpx();
      cc.mov(callUpvaluesReg, (uint64_t)&closeUpvalues);
      InvokeNode* call_closeUpValues;
      cc.invoke(&call_closeUpValues, callUpvaluesReg, FuncSignatureT<void, Value*>());
      call_closeUpValues->setArg(0, slotBasePtr);

      // vm.frameCount--;
      auto frameCountReg = cc.newGpw();
      auto frameCountAddrReg = cc.newUIntPtr();
      cc.mov(frameCountAddrReg, (uint64_t)&vm.frameCount);
      cc.ldr(frameCountReg, a64::Mem(frameCountAddrReg));
      cc.sub(frameCountReg, frameCountReg, 1);
      cc.str(frameCountReg, a64::Mem(frameCountAddrReg));

      PEEK(resultReg, 0);
      POP();
      FLUSH_REGISTER_CACHE();

      auto retReg = cc.newGpx();
      cc.mov(retReg, INTERPRET_OK);
      auto frameCountNot0 = cc.newLabel();
      cc.cbnz(frameCountReg, frameCountNot0);

      // if frame count == 0
      POP();
      cc.ret(retReg);

      // otherwise
      cc.bind(frameCountNot0);
      // vm.stackTop = frame->slots;
      cc.str(slotBasePtr, a64::Mem(stackTopPtrReg));
      PUSH_REG(resultReg);
      FLUSH_REGISTER_CACHE();
      cc.ret(retReg);

      break;
    }
    default:
      printf("Unsupported opcode: %d\n", op);
      exit(1);
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef PEEK_COMPILE_CONSTANT
#undef PUSH_SETUP
#undef PUSH_MEM
#undef PUSH_REG
#undef PEEK
#undef POP

  cc.ret();
  cc.endFunc();
  cc.finalize();

  JittedFn fn;
  Error err = rt.add(&fn, &code);
  if (err)
  {
    printf("Error: %s\n", DebugUtils::errorAsString(err));
    return NULL;
  }

  jittedFns.push_back(fn);

  return fn;
}
