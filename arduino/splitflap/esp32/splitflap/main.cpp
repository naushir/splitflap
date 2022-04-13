/*
   Copyright 2020 Scott Bezek and the splitflap contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include "config.h"

#include "../core/splitflap_task.h"
#include "clock_task.h"
#include "display_task.h"
#include "serial_task.h"
#include "ota_task.h"

SplitflapTask splitflapTask(1, LedMode::AUTO);
SerialTask serialTask(splitflapTask, 0);

#if ENABLE_DISPLAY
DisplayTask displayTask(splitflapTask, 0);
#endif

#if ENABLE_OTA
OtaTask otaTask(serialTask, 0);
#endif

ClockTask clockTask(splitflapTask, displayTask, serialTask, 0);

void setup() {
  serialTask.begin();

  splitflapTask.begin();

  #if ENABLE_DISPLAY
  displayTask.begin();
  #endif

  clockTask.begin();

  #if ENABLE_OTA
  otaTask.begin();
  #endif


  // Delete the default Arduino loopTask to free up Core 1
  vTaskDelete(NULL);
}


void loop() {
  assert(false);
}
