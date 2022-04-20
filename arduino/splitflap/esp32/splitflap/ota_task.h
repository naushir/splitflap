#pragma once

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "../core/logger.h"
#include "../core/task.h"

class OtaTask : public Task<OtaTask>
{
    friend class Task<OtaTask>; // Allow base Task to invoke protected run()

    public:
        OtaTask(Logger& logger, const uint8_t task_core) :
                Task("OTA", 8192, 1, task_core),
                logger_(logger)
        {
        }
            
    protected:
        void run()
        {
            char buf[128];

            // Wait for WiFi to be up and connected
            while (WiFi.status() != WL_CONNECTED)
                delay(1000);

            // From https://github.com/espressif/arduino-esp32/blob/master/libraries/ArduinoOTA/examples/BasicOTA/BasicOTA.ino
            // Port defaults to 3232
            // ArduinoOTA.setPort(3232);

            // Hostname defaults to esp3232-[MAC]
            // ArduinoOTA.setHostname("clock");

            // No authentication by default
            // ArduinoOTA.setPassword("admin");

            // Password can be set with it's md5 value as well
            // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
            // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
            
            ArduinoOTA
                .onStart([this, &buf]() {
                    const char *type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                    snprintf(buf, sizeof(buf), "Start updating %s", type);
                    logger_.log(buf);
                })

                .onEnd([this]() {
                    logger_.log("OTA End");
                })

                .onProgress([this, &buf](unsigned int progress, unsigned int total) {
                    snprintf(buf, sizeof(buf), "Progress: %u%%\r", (progress / (total / 100)));
                    logger_.log(buf);
                })

                .onError([this, &buf](ota_error_t error) {
                    const char *err = "";
                    if (error == OTA_AUTH_ERROR) err  = "Auth Failed";
                    else if (error == OTA_BEGIN_ERROR) err = "Begin Failed";
                    else if (error == OTA_CONNECT_ERROR) err = "Connect Failed";
                    else if (error == OTA_RECEIVE_ERROR) err = "Receive Failed";
                    else if (error == OTA_END_ERROR) err = "End Failed";
                    snprintf(buf, sizeof(buf), "Error[%u]: %s", error, err);
                    logger_.log(buf);
                });

            ArduinoOTA.begin();

            while (true)
            {
                ArduinoOTA.handle();
                delay(100);
            }
        }

    private:
        Logger& logger_;
};
