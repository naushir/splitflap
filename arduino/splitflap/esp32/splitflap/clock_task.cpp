#include "clock_task.h"

#include <lwip/apps/sntp.h>
#include <json11.hpp>

#include "secrets.h"

// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"

ClockTask::ClockTask(SplitflapTask& splitflap_task, DisplayTask& display_task, Logger& logger, const uint8_t task_core) :
        Task("Clock", 8192, 1, task_core),
        splitflap_task_(splitflap_task),
        display_task_(display_task),
        logger_(logger),
        wifi_client_(),
        last_(0)
{
}

void ClockTask::init()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    char buf[256];

    logger_.log("Establishing connection to WiFi..");
    snprintf(buf, sizeof(buf), "Wifi connecting to %s", WIFI_SSID);
    display_task_.setMessage(1, String(buf));
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }

    snprintf(buf, sizeof(buf), "Connected to network %s", WIFI_SSID);
    logger_.log(buf);

    // Sync SNTP
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    char server[] = "pool.ntp.org";
    sntp_setservername(0, server);
    //sntp_set_sync_interval(15 * 60 * 1000);
    sntp_init();

    logger_.log("Waiting for NTP time sync...");
    snprintf(buf, sizeof(buf), "Syncing NTP time via %s...", server);
    display_task_.setMessage(1, String(buf));
    time_t now;
    while (time(&now), now < 1625099485) {
        delay(1000);
    }

    setenv("TZ", TIMEZONE, 1);
    tzset();
    strftime(buf, sizeof(buf), "Got time: %Y-%m-%d %H:%M:%S", localtime(&now));
    logger_.log(buf);
}

void ClockTask::showClock(time_t now)
{
    char buf[NUM_MODULES];

    if (last_ != now) {
        const struct tm *lt = localtime(&now);

        strftime(buf, sizeof(buf), "%I.%M", lt);
        buf[NUM_MODULES-1] = lt->tm_hour >=12 ? 'p' : 'a';

        splitflap_task_.showString(buf, NUM_MODULES, false);
        last_ = now;
    }
}

void ClockTask::run()
{
    init();

    String wifi_status;
    switch (WiFi.status()) {
        case WL_IDLE_STATUS:
            wifi_status = "Idle";
            break;
        case WL_NO_SSID_AVAIL:
            wifi_status = "No SSID";
            break;
        case WL_CONNECTED:
            wifi_status = String(WIFI_SSID) + " " + WiFi.localIP().toString();
            break;
        case WL_CONNECT_FAILED:
            wifi_status = "Connection failed";
            break;
        case WL_CONNECTION_LOST:
            wifi_status = "Connection lost";
            break;
        case WL_DISCONNECTED:
            wifi_status = "Disconnected";
            break;
        default:
            wifi_status = "Unknown";
            break;
    }
    display_task_.setMessage(1, String("Wifi: ") + wifi_status);

    while(1) {
        time_t now;

        time(&now);
        showClock(now);

        delay(250);
    }
}