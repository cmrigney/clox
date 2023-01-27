
#include <stdlib.h>
#include "pico/stdlib.h"
#include "../clox.h"
#include "rf_95.h"
#include "autogen/lora_lib.h"

static void rf95_destroy(void *rf95) {
    rf95_free(rf95);
}

static Value create(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("create() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[0])) {
        // runtimeError("create() argument must be a number.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("create() argument must be a number.");
        return NIL_VAL;
    }
    uint reset_pin = (uint)AS_NUMBER(args[0]);
    uint interrupt_pin = (uint)AS_NUMBER(args[1]);
    RF95 *rf95 = rf95_create(reset_pin, interrupt_pin);
    if(rf95 == NULL) {
      // runtimeError("create() failed to create RF95 object.");
      return NIL_VAL;
    }
    ObjRef *ref = newRef("rf95", "RF95 Module", rf95, rf95_destroy);
    return OBJ_VAL(ref);
}

static Value disable(Value *receiver, int argCount, Value *args) {
    if(argCount != 1) {
        // runtimeError("disable() takes exactly 1 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("disable() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("disable() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    rf95_deinit(rf95);
    return NIL_VAL;
}

static Value setTxPower(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("setTxPower() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("setTxPower() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("setTxPower() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("setTxPower() argument must be a number.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    int8_t power = (int8_t)AS_NUMBER(args[1]);
    rf95_setTxPower(rf95, power);
    return NIL_VAL;
}

static Value setFrequency(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("setFrequency() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("setFrequency() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("setFrequency() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("setFrequency() argument must be a number.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    float frequency = (float)AS_NUMBER(args[1]);
    rf95_setFrequency(rf95, frequency);
    return NIL_VAL;
}

static Value setThisAddress(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("setThisAddress() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("setThisAddress() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("setThisAddress() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("setThisAddress() argument must be a number.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    uint8_t address = (uint8_t)AS_NUMBER(args[1]);
    rf95->thisAddress = address;
    return NIL_VAL;
}

static Value setHeaderFrom(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("setHeaderFrom() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("setHeaderFrom() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("setHeaderFrom() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("setHeaderFrom() argument must be a number.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    uint8_t address = (uint8_t)AS_NUMBER(args[1]);
    rf95->txHeaderFrom = address;
    return NIL_VAL;
}

static Value setHeaderTo(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("setHeaderTo() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("setHeaderTo() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("setHeaderTo() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_NUMBER(args[1])) {
        // runtimeError("setHeaderTo() argument must be a number.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    uint8_t address = (uint8_t)AS_NUMBER(args[1]);
    rf95->txHeaderTo = address;
    return NIL_VAL;
}

static Value available(Value *receiver, int argCount, Value *args) {
    if(argCount != 1) {
        // runtimeError("available() takes exactly 1 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("available() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("available() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    return BOOL_VAL(rf95_available(rf95));
}

static Value recv(Value *receiver, int argCount, Value *args) {
    if(argCount != 1) {
        // runtimeError("recv() takes exactly 1 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("recv() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("recv() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if(!rf95_recv(rf95, buf, &len)) {
        return NIL_VAL;
    }
    ObjBuffer *buffer = newBuffer((int)len);
    memcpy(buffer->bytes, buf, len);
    return OBJ_VAL(buffer);
}

static Value send(Value *receiver, int argCount, Value *args) {
    if(argCount != 2) {
        // runtimeError("send() takes exactly 2 argument (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("send() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("send() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    if(!IS_BUFFER(args[1])) {
        // runtimeError("send() argument must be a buffer.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    ObjBuffer *buffer = AS_BUFFER(args[1]);
    return BOOL_VAL(rf95_send(rf95, buffer->bytes, buffer->size));
}

static Value waitForPacketSent(Value *receiver, int argCount, Value *args) {
    if(argCount < 1 || argCount > 2) {
        // runtimeError("waitForPacketSent() takes 1-2 arguments (%d given).", argCount);
        return NIL_VAL;
    }
    if(!IS_REF(args[0])) {
        // runtimeError("waitForPacketSent() argument must be an object reference.");
        return NIL_VAL;
    }
    if(strcmp(AS_REF(args[0])->magic, "rf95") != 0) {
        // runtimeError("waitForPacketSent() argument must be an object reference of type 'rf95'.");
        return NIL_VAL;
    }
    RF95 *rf95 = (RF95*)AS_REF(args[0])->data;
    if(argCount == 2 && IS_NUMBER(args[1])) {
        return BOOL_VAL(rf95_waitPacketSentTimeout(rf95, (uint16_t)AS_NUMBER(args[1])));
    }
    return BOOL_VAL(rf95_waitPacketSent(rf95));
}

bool registerModule_lora_radio() {
  registerNativeMethod("__create_rf95", create);
  registerNativeMethod("__disable_rf95", disable);
  registerNativeMethod("__set_tx_power_rf95", setTxPower);
  registerNativeMethod("__set_frequency_rf95", setFrequency);
  registerNativeMethod("__set_this_address_rf95", setThisAddress);
  registerNativeMethod("__set_header_to_rf95", setHeaderTo);
  registerNativeMethod("__set_header_from_rf95", setHeaderFrom);
  registerNativeMethod("__available_rf95", available);
  registerNativeMethod("__recv_rf95", recv);
  registerNativeMethod("__send_rf95", send);
  registerNativeMethod("__wait_for_packet_sent_rf95", waitForPacketSent);
  char *code = strndup(lora_module_bundle, lora_module_bundle_len);
  registerLoxCode(code);
  free(code);
  return true;
}

