// ===========================================================================
//  main.cpp -- composition root entry point.
//
//  All work happens inside the two FreeRTOS tasks created by App; loop() is
//  intentionally idle.
// ===========================================================================
#include <Arduino.h>

#include "app/app.h"

static App g_app;

void setup() {
  g_app.begin();
  g_app.startTasks();
}

void loop() {
  // Everything runs in uiTask / netTask.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
