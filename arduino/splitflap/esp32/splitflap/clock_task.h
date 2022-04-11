#pragma once

#include <time.h>

#include <Arduino.h>
#include <WiFi.h>

#include "../core/logger.h"
#include "../core/splitflap_task.h"
#include "../core/task.h"

#include "display_task.h"

class ClockTask : public Task<ClockTask>
{
    friend class Task<ClockTask>; // Allow base Task to invoke protected run()

    public:
        ClockTask(SplitflapTask& splitflap_task, DisplayTask& display_task, Logger& logger, const uint8_t task_core);

    protected:
        void run();

    private:
        void init();
        void showClock(time_t now);

        SplitflapTask& splitflap_task_;
        DisplayTask& display_task_;
        Logger& logger_;
        WiFiClient wifi_client_;
        time_t last_;
};