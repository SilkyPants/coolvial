#include "WebManager.h"

#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "FileSystem.h"

#define FILE_HOME_INDEX "index.html"
#define FILE_LOGS "logs.csv"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

WebManager::WebManager(TemperatureManager *tm, ThermalController *tc)
    : tempMgr(tm), thermalCtrl(tc)
{
    server = new AsyncWebServer(80);
}

void WebManager::begin()
{
    timeClient.begin();
    timeClient.update();

    setupRoutes();
    server->begin();
}

void WebManager::setupRoutes()
{
    server->on("/config", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
    // Explicitly open the file from your StaticFS object
    if (StaticFS.exists("/config.html")) {
        request->send(StaticFS, "/config.html", "text/html");
    } else {
        request->send(404, "text/plain", "Config page not found on filesystem.");
    } });

    // 2. Serve the CSV file directly from StorageFS
    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        // AsyncWebServer handles the file streaming in the background automatically
        request->send(StorageFS, "/" FILE_LOGS, "text/csv"); });

    server->on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest *request)
               {
        if (StorageFS.remove("/" FILE_LOGS)) {
            request->send(200, "application/json", "{\"status\":\"cleared\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"failed to delete\"}");
        } });

    // 3a. The Dynamic JSON API endpoint
    server->on("/api/data", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        
        Temperatures live = tempMgr->readStored();
        Temperatures avg = tempMgr->getRollingAverage();

        doc["timestamp"] = lastLoggedTime;          // The timestamp of the ACTUAL last save
        doc["lastLoggedTemps"] = lastLoggedTemps;

        doc["current"] = live;
        doc["average"] = avg;
        
        doc["status"] = "Active";

        serializeJson(doc, *response);
        request->send(response); });

    // 3b. Handle Config Save (Post Request)
    // --- GET Config ---
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        
        // Fetch current active values from your ThermalController
        doc["setpoint"] = thermalCtrl->getSetpoint();
        doc["kp"] = thermalCtrl->getKp();
        doc["ki"] = thermalCtrl->getKi();
        doc["kd"] = thermalCtrl->getKd();
        
        serializeJson(doc, *response);
        request->send(response); });

    // --- POST Config ---
    // In AsyncWebServer, POST bodies are handled in a separate callback
    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
               {
        // This is required to acknowledge the request, actual data is handled below
        request->send(200, "application/json", "{\"status\":\"success\"}"); }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
               {
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            float sp = doc["setpoint"];
            float p = doc["kp"];
            float i = doc["ki"];
            float d = doc["kd"];
            
            // Push active updates to the PID controller
            thermalCtrl->updateTunings(p, i, d);
            thermalCtrl->setSetpoint(sp);

            // Save to NVS
            Preferences prefs;
            prefs.begin("coolvial-conf", false);
            prefs.putFloat("setpoint", sp);
            prefs.putFloat("kp", p);
            prefs.putFloat("ki", i);
            prefs.putFloat("kd", d);
            prefs.end();
            
            Serial.printf("Config Updated: SP=%.1f, Kp=%.1f, Ki=%.1f, Kd=%.1f\n", sp, p, i, d);
        } });

    // 3c. Handle assignment of temp probe roles
    // 1. Start the discovery process
    server->on("/api/assign/start", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
    if (request->hasParam("roleIndex", true)) {
        int idx = request->getParam("roleIndex", true)->value().toInt();
        
        // Validate index to prevent memory access issues
        if (idx >= 0 && idx <= 2) {
            tempMgr->startDiscovery((Role)idx);
            request->send(200, "application/json", "{\"status\":\"started\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"invalid role index\"}");
        }
    } else {
        request->send(400, "application/json", "{\"error\":\"missing roleIndex\"}");
    } });

    // 2. Poll for the status
    server->on("/api/assign/status", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
    // Map internal DiscoveryState to strings for the frontend
    const char* stateStrings[] = {"idle", "searching", "success", "failed"};
    
    DiscoveryState s = tempMgr->getDiscoveryState();
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    JsonDocument doc;
    doc["status"] = stateStrings[(int)s]; 
    
    serializeJson(doc, *response);
    request->send(response); });

    // 4. Handle 404s gracefully
    server->onNotFound([](AsyncWebServerRequest *request)
                       {
        if (request->method() == HTTP_OPTIONS) {
        request->send(200); // Handle CORS preflight if needed later
        } else {
        request->send(404, "text/plain", "Not Found");
        } });

    // Map URL /static/index.html -> FS file /index.html
    server->serveStatic("/style.css", StaticFS, "/style.css");
    server->serveStatic("/script.js", StaticFS, "/script.js");
    server->serveStatic("/config.js", StaticFS, "/config.js");

    server->serveStatic("/", StaticFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=600");
}

void WebManager::logDataToFS(Temperatures t)
{
    File file = StorageFS.open("/" FILE_LOGS, FILE_APPEND);
    if (file)
    {

        unsigned long now = timeClient.getEpochTime();
        // Corrected CSV format: timestamp,temp (no extra newlines between columns)
        file.printf("%lu,%.2f,%.2f,%.2f\n", now, t.blockTemp, t.internalTemp, t.ambientTemp);
        file.close();

        // Store off last events
        lastLoggedTemps = t;
        lastLoggedTime = now;
    }
}