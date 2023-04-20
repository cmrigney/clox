#include <string.h>
#include <math.h>
#include <vector>
#include <cstdlib>

#include "libraries/pico_display/pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "rgbled.hpp"

#include "../clox.h"
#include "autogen/pimoroni_lib.h"

using namespace pimoroni;

ST7789 st7789(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);

// RGBLED led(PicoDisplay::LED_R, PicoDisplay::LED_G, PicoDisplay::LED_B);

static Value screen_set_backlight(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("set_backlight requires 1 argument");
    return NIL_VAL;
  }

  if(!IS_NUMBER(args[0])) {
    // runtimeError("set_backlight requires a number");
    return NIL_VAL;
  }

  st7789.set_backlight((uint8_t)(NUMBER_VAL(args[0]) * 255.0f));
  return NIL_VAL;
}

static Value screen_clear_screen(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("clear_screen requires 0 arguments");
    return NIL_VAL;
  }

  graphics.clear();
  return NIL_VAL;
}

static Value screen_create_pen(Value *receiver, int argCount, Value *args) {
  if(argCount != 3) {
    // runtimeError("create_pen requires 3 arguments");
    return NIL_VAL;
  }

  if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
    // runtimeError("create_pen requires 3 numbers");
    return NIL_VAL;
  }

  int pen = graphics.create_pen(AS_NUMBER(args[0]), AS_NUMBER(args[1]), AS_NUMBER(args[2]));

  return OBJ_VAL(newRef("pim_pen", "pen", (void*)pen, NULL));
}

static Value screen_set_pen(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("set_pen requires 1 argument");
    return NIL_VAL;
  }

  if(!IS_REF(args[0])) { // TODO check magic
    // runtimeError("set_pen requires a number");
    return NIL_VAL;
  }

  graphics.set_pen((int)(AS_REF(args[0])->data));
  return NIL_VAL;
}

static Value screen_set_thickness(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("set_thickness requires 1 argument");
    return NIL_VAL;
  }

  if(!IS_NUMBER(args[0])) {
    // runtimeError("set_thickness requires a number");
    return NIL_VAL;
  }

  graphics.set_thickness((uint)NUMBER_VAL(args[0]));
  return NIL_VAL;
}

static Value screen_text(Value *receiver, int argCount, Value *args) {
  if(argCount != 3) {
    // runtimeError("text requires 3 arguments");
    return NIL_VAL;
  }

  if(!IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
    // runtimeError("text requires a string, a number and a number");
    return NIL_VAL;
  }

  graphics.text(AS_CSTRING(args[0]), Point((int)NUMBER_VAL(args[1]), (int)NUMBER_VAL(args[2])), 240 - NUMBER_VAL(args[1]));
  return NIL_VAL;
}

static Value screen_update(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("update requires 0 arguments");
    return NIL_VAL;
  }

  st7789.update(&graphics);
  return NIL_VAL;
}

static Value test(Value *receiver, int argCount, Value *args) {
  st7789.set_backlight(255);
  struct pt {
    float      x;
    float      y;
    uint8_t    r;
    float     dx;
    float     dy;
    uint16_t pen;
  };

  std::vector<pt> shapes;
  for(int i = 0; i < 100; i++) {
    pt shape;
    shape.x = rand() % 240;
    shape.y = rand() % 135;
    shape.r = (rand() % 10) + 3;
    shape.dx = float(rand() % 255) / 128.0f;
    shape.dy = float(rand() % 255) / 128.0f;
    shape.pen = graphics.create_pen(rand() % 255, rand() % 255, rand() % 255);
    shapes.push_back(shape);
  }

  uint32_t i = 0;
  Pen BG = graphics.create_pen(120, 40, 60);
  Pen YELLOW = graphics.create_pen(255, 255, 0);
  Pen TEAL = graphics.create_pen(0, 255, 255);
  Pen WHITE = graphics.create_pen(255, 255, 255);
  Pen TEXT = graphics.create_pen(255, 50, 50);

  while(true) {
    graphics.set_pen(BG);
    graphics.clear();

    for(auto &shape : shapes) {
      shape.x += shape.dx;
      shape.y += shape.dy;
      if(shape.x < 0) shape.dx *= -1;
      if(shape.x >= graphics.bounds.w) shape.dx *= -1;
      if(shape.y < 0) shape.dy *= -1;
      if(shape.y >= graphics.bounds.h) shape.dy *= -1;

      graphics.set_pen(shape.pen);
      graphics.circle(Point(shape.x, shape.y), shape.r);
    }

    float led_step = fmod(i / 20.0f, M_PI * 2.0f);
    int r = (sin(led_step) * 32.0f) + 32.0f;
    // led.set_rgb(r, r / 1.2f, r);


    std::vector<Point> poly;
    poly.push_back(Point(30, 30));
    poly.push_back(Point(50, 35));
    poly.push_back(Point(70, 25));
    poly.push_back(Point(80, 65));
    poly.push_back(Point(50, 85));
    poly.push_back(Point(30, 45));

    graphics.set_pen(YELLOW);
    //pico_display.pixel(Point(0, 0));
    graphics.polygon(poly);

    graphics.set_pen(TEAL);
    graphics.triangle(Point(50, 50), Point(130, 80), Point(80, 110));

    graphics.set_pen(WHITE);
    graphics.line(Point(50, 50), Point(120, 80));
    graphics.line(Point(20, 20), Point(120, 20));
    graphics.line(Point(20, 20), Point(20, 120));

    for(int r = 0; r < 30; r++) {
      for(int j = 0; j < 10; j++) {
        float rads = ((M_PI * 2) / 30.0f) * float(r);
        rads += (float(i) / 100.0f);
        rads += (float(j) / 100.0f);
        float cx = sin(rads) * 300.0f;
        float cy = cos(rads) * 300.0f;
        graphics.line(Point(120, 67), Point(cx + 120, cy + 67));
      }
    }

    graphics.set_pen(TEXT);
    graphics.set_thickness(2);
    graphics.text("Hello World", Point(10, 10), 250);

    // update screen
    st7789.update(&graphics);
    sleep_ms(1000 / 60);
    i++;
  }
  return NIL_VAL;
}

extern "C" {
  bool registerModule_pimoroni() {
    registerNativeMethod("__test", test);
    registerNativeMethod("__set_backlight", screen_set_backlight);
    registerNativeMethod("__clear", screen_clear_screen);
    registerNativeMethod("__create_pen", screen_create_pen);
    registerNativeMethod("__set_pen", screen_set_pen);
    registerNativeMethod("__set_thickness", screen_set_thickness);
    registerNativeMethod("__text", screen_text);
    registerNativeMethod("__update", screen_update);
    char *code = strndup((char*)pimoroni_module_bundle, pimoroni_module_bundle_len);
    registerLoxCode(code);
    free(code);
    return true;
  }
}
