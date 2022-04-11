#include "clock_task.h"

#include "esp_sntp.h"
#include "secrets.h"

// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"

ClockTask::ClockTask(SplitflapTask& splitflap_task, DisplayTask& display_task, Logger& logger, const uint8_t task_core) :
        Task("Clock", 8192, 1, task_core),
        splitflap_task_(splitflap_task),
        display_task_(display_task),
        logger_(logger),
        wifi_client_(),
        lastTime_(0), lastCalibration_(0)
{
}

void ClockTask::connectWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    char buf[256];

    splitflap_task_.showString("WIFI. ", NUM_MODULES, true);
    logger_.log("Establishing connectWiFiion to WiFi..");
    snprintf(buf, sizeof(buf), "Wifi connectWiFiing to %s", WIFI_SSID);
    display_task_.setMessage(1, String(buf));
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }

    splitflap_task_.showString("READY.", NUM_MODULES, true);
    snprintf(buf, sizeof(buf), "ConnectWiFied to network %s", WIFI_SSID);
    logger_.log(buf);
}

void ClockTask::syncNTP()
{
    char buf[256];

    // Sync SNTP
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_setservername(1, "pool.ntp.org");
    sntp_set_sync_interval(15 * 60 * 1000);
    sntp_init();
    delay(2000);

    splitflap_task_.showString("SYNC  ", NUM_MODULES, true);
    logger_.log("Waiting for NTP time sync...");
    display_task_.setMessage(1, "Syncing NTP time");

    // Wait for the time to be set.
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        snprintf(buf, sizeof(buf), "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        logger_.log(buf);
        snprintf(buf, sizeof(buf), "SYNC%d", retry);
        splitflap_task_.showString(buf, NUM_MODULES, false);
        delay(2000);
    }

    time_t now = 0;
    struct tm timeinfo = { 0 };

    setenv("TZ", TIMEZONE, 1);
    tzset();

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "Got time: %Y-%m-%d %H:%M:%S", &timeinfo);
    logger_.log(buf);
}

void ClockTask::showClock(time_t now)
{
    char buf[NUM_MODULES];
    struct tm ti = { 0 };
    struct tm lti = { 0 };
    localtime_r(&now, &ti);
    localtime_r(&lastTime_, &lti);

    if (lti.tm_sec != ti.tm_sec || lti.tm_min != ti.tm_min || lti.tm_hour != ti.tm_hour) {

        if (NUM_MODULES == 6)
        {
            strftime(buf, sizeof(buf), "%I.%M", &ti);
            buf[NUM_MODULES-1] = (ti.tm_hour >= 12) ? 'P' : 'A';
        }
        else if (NUM_MODULES == 4)
        {
            strftime(buf, sizeof(buf), "%I%M", &ti);
        }

        splitflap_task_.showString(buf, NUM_MODULES, false);
        lastTime_ = now;
    }
}

void ClockTask::checkRecalibration()
{
    unsigned long now = millis();
    if (now - lastCalibration_ > 44 * 60 * 1000) {
        splitflap_task_.resetAll();
        lastCalibration_ = now;
    }
}

void ClockTask::run()
{
    connectWiFi();
    syncNTP();

    while(1) {
        time_t now;

        time(&now);
        showClock(now);

        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            connectWiFi();
        }

        checkRecalibration();
        delay(250);
    }
}