#include "clock_task.h"

#include "esp_sntp.h"

// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"

int sleepStart = 23;
int sleepEnd = 6;
int dateMin = 24;

const int buttonPin = 12;
const int ledR = 2;
const int ledG = 15;
const int ledB = 13;

JLed whiteBreathe[3] = {
    JLed(ledR).Breathe(4500).LowActive().MaxBrightness(0.2 * 256),
    JLed(ledG).Breathe(4500).LowActive().MaxBrightness(0.5 * 256),
    JLed(ledB).Breathe(4500).LowActive().MaxBrightness(0.9 * 256)
};

JLed redBreathe[3] = {
    JLed(ledR).Breathe(4500).LowActive().MaxBrightness(0.8 * 256),
    JLed(ledG).Breathe(4500).LowActive().MaxBrightness(0),
    JLed(ledB).Breathe(4500).LowActive().MaxBrightness(0)
};

JLed greenBreathe[3] = {
    JLed(ledR).Breathe(4500).LowActive().MaxBrightness(0),
    JLed(ledG).Breathe(4500).LowActive().MaxBrightness(0.8 * 256),
    JLed(ledB).Breathe(4500).LowActive().MaxBrightness(0)
};

JLed blueBlink[3] = {
    JLed(ledR).Blink(100, 100).LowActive().MaxBrightness(0),
    JLed(ledG).Blink(100, 100).LowActive().MaxBrightness(0),
    JLed(ledB).Blink(100, 100).LowActive().MaxBrightness(0.8 * 256),
};

JLed yellowBlink[3] = {
    JLed(ledR).Blink(250, 250).LowActive().MaxBrightness(0.4 * 256),
    JLed(ledG).Blink(250, 250).LowActive().MaxBrightness(0.9 * 256),
    JLed(ledB).Blink(250, 250).LowActive().MaxBrightness(0),
};

ClockTask::ClockTask(SplitflapTask& splitflap_task, DisplayTask& display_task, Logger& logger, const uint8_t task_core) :
        Task("Clock", 8192, 1, task_core),
        splitflap_task_(splitflap_task),
        display_task_(display_task),
        logger_(logger),
        lastTime_(0), lastCalibration_(0),
        sleep_(false), sleepToggle_(false),
        button_(buttonPin, true, true),
        leds_(JLedSequence(JLedSequence::eMode::PARALLEL, yellowBlink).Forever())
{
        button_.setPressTicks(5000);
        button_.attachDuringLongPress([](void *p) {
            ((ClockTask *)p)->logger_.log("Reset press");
            ((ClockTask *)p)->reset();
        }, this);

        button_.attachClick([](void *p){
            ((ClockTask *)p)->logger_.log("Sleep press");
            ((ClockTask *)p)->sleepToggle_ = true;
        }, this);
}

void ClockTask::connectWiFi()
{
    char buf[256];

    splitflap_task_.showString("wifi  ", NUM_MODULES, true);
    logger_.log("Establishing connection to WiFi");

    WiFi.mode(WIFI_STA);
    wifiManager_.setConfigPortalBlocking(false);
    WiFiManagerParameter sleepTime("sleep_time", "Sleep Time (hr)", "23", 6);
    WiFiManagerParameter wakeTime("wake_time", "Wake Time (hr)", "06", 6);
    WiFiManagerParameter dateDisplay("date_display", "Date display (min)", "24", 6);
    wifiManager_.addParameter(&sleepTime);
    wifiManager_.addParameter(&wakeTime);
    wifiManager_.addParameter(&dateDisplay);
    wifiManager_.autoConnect("Splitflap", "splitflap");

    while (WiFi.status() != WL_CONNECTED) {
        wifiManager_.process();
        wait(1);
    }

    sleepStart = atoi(sleepTime.getValue());
    sleepEnd = atoi(wakeTime.getValue());
    dateMin = atoi(dateDisplay.getValue());
    snprintf(buf, sizeof(buf), "Sleep time %d/%d : Date display %d", sleepStart, sleepEnd, dateMin);
    logger_.log(buf);

    splitflap_task_.showString("ready", NUM_MODULES, true);
    snprintf(buf, sizeof(buf), "Connected to network %s", wifiManager_.getWiFiSSID().c_str());
    display_task_.setMessage(1, String(buf));
    logger_.log(buf);
}

void ClockTask::syncNTP()
{
    char buf[256];

    if (sntp_enabled())
        sntp_stop();

    // Sync SNTP
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    sntp_setservername(0, (char *)"pool.ntp.org");
    sntp_setservername(1, (char *)"europe.pool.ntp.org");
    //sntp_set_sync_interval(15 * 60 * 1000);
    sntp_init();
    wait(2000);

    splitflap_task_.showString("sync  ", NUM_MODULES, true);
    logger_.log("Waiting for NTP time sync...");
    display_task_.setMessage(1, "Syncing NTP time");

    // Wait for the time to be set.
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        snprintf(buf, sizeof(buf), "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        logger_.log(buf);
        snprintf(buf, sizeof(buf), "sync%02d", retry);
        splitflap_task_.showString(buf, NUM_MODULES, false);
        wait(2000);
    }

    time_t now = 0;
    struct tm timeinfo = { 0 };

    setenv("TZ", TIMEZONE, 1);
    tzset();

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "Sync time: %Y-%m-%d %H:%M:%S", &timeinfo);
    display_task_.setMessage(1, buf);
    logger_.log(buf);
}

void ClockTask::showClock(time_t now)
{
    char buf[NUM_MODULES + 1];
    struct tm ti = { 0 };
    struct tm lti = { 0 };

    localtime_r(&now, &ti);
    localtime_r(&lastTime_, &lti);

    if (lti.tm_sec != ti.tm_sec || lti.tm_min != ti.tm_min || lti.tm_hour != ti.tm_hour) {

        strftime(buf, sizeof(buf), "%I%M", &ti);
        if (NUM_MODULES == 6)
            snprintf(buf + 4, sizeof(buf) - 4, "%s", (ti.tm_hour >= 12) ? "pm" : "am");

        splitflap_task_.showString(buf, NUM_MODULES, false);
        lastTime_ = now;
    }
}

void ClockTask::showDate(time_t now)
{
    char buf[NUM_MODULES + 1];
    struct tm ti = { 0 };

    if (!dateMin)
        return;

    localtime_r(&now, &ti);

    if (ti.tm_min && (ti.tm_min % dateMin == 0) && (ti.tm_sec == 4)) {

        if (NUM_MODULES == 6)
            strftime(buf, sizeof(buf), "%d%m%y", &ti);
        else
            strftime(buf, sizeof(buf), "%d%m", &ti);

        splitflap_task_.showString(buf, NUM_MODULES, false);
        wait(30 * 1000);
    }
}

void ClockTask::updateState(time_t now)
{
    struct tm ti = { 0 };

    localtime_r(&now, &ti);

    if (!sleep_ && (sleepToggle_ || (ti.tm_hour >= sleepStart || ti.tm_hour < sleepEnd)))
    {
        logger_.log("Entering sleep");
        splitflap_task_.showString("      ", NUM_MODULES, false);
        setLED(redBreathe);
        sleep_ = true;
        sleepToggle_ = false;
    }
    else if (sleep_ && (sleepToggle_ || (ti.tm_hour >= sleepEnd && ti.tm_hour < sleepStart)))
    {
        logger_.log("Waking from sleep");
        setLED(whiteBreathe);
        sleep_ = false;
        sleepToggle_ = false;
    }
}

void ClockTask::checkRecalibration()
{
    unsigned long now = millis();
    if (now - lastCalibration_ > 144 * 60 * 1000) {
        splitflap_task_.resetAll();
        lastCalibration_ = now;
    }
}

void ClockTask::reset()
{
    setLED(blueBlink);
    wait(5000, false);

    wifiManager_.resetSettings();
    logger_.log("Restarting...");
    ESP.restart();
}

void ClockTask::wait(unsigned long msec, bool buttonUpdate)
{
    unsigned long start = millis();
    do
    {
        if (buttonUpdate)
            button_.tick();
        leds_.Update();
        delay(1);
    } while (millis() - start < msec);
}

void ClockTask::setLED(JLed seq[3])
{
    leds_.Stop();
    leds_ = JLedSequence(JLedSequence::eMode::PARALLEL, seq, 3).Forever();
}

void ClockTask::run()
{
    lastCalibration_ = millis();
    connectWiFi();
    syncNTP();

    setLED(whiteBreathe);

    while (1) {
        time_t now;
        time(&now);

        if (!sleep_) {
            showClock(now);
            showDate(now);
            checkRecalibration();

            if (WiFi.status() != WL_CONNECTED) {
                WiFi.disconnect();
                connectWiFi();
            }
        }

        updateState(now);
        wait(1);
    }
}