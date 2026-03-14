#pragma once
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "TemperatureManager.h"
#include "ThermalController.h"

class WebManager {
private:
    AsyncWebServer* server;
    // Pointers to other managers so the API can fetch live data
    TemperatureManager* tempMgr; 
    ThermalController* thermalCtrl;

    // Legacy properties for now
    Temperatures lastLoggedTemps;
    unsigned long lastLoggedTime = 0;

public:
    WebManager(TemperatureManager* tm, ThermalController* tc);
    void begin();
    void setupRoutes();
    void logDataToFS(Temperatures t);
};