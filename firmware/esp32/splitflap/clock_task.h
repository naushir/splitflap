#pragma once

#include <time.h>

#include <Arduino.h>
#include <jled.h>
#include <OneButton.h>
#include <WiFiManager.h>

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
        void provision();
        void syncNTP();
        void showClock(time_t now);
        void showDate(time_t now);
        void updateState(time_t now);
        void checkRecalibration();
        void reset();
        void wait(unsigned long msec, bool buttonUpdate = true);
        void setLED(JLed seq[3]);

        SplitflapTask& splitflap_task_;
        DisplayTask& display_task_;
        Logger& logger_;
        time_t lastTime_;
        unsigned long lastCalibration_;
        bool sleep_;
        bool sleepToggle_;
        OneButton button_;
        JLedSequence leds_;
        WiFiManager wifiManager_;
};
