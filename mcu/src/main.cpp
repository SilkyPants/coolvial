
#include <Arduino.h>

#include "FileSystem.h"
#include "TemperatureManager.h"
#include "ThermalController.h"
#include "WebManager.h"
#include "DisplayManager.h"

// Pins for DS18B20 bus
#define SENSORS_PIN 4

#define FAN_TACH 3
#define FAN_PWM 2

#define PELTIER_INT1 1
#define PELTIER_INT2 0

#define SCREEN_PIN 11

// Pins for OLED
#define OLED_SDA 7
#define OLED_SCL 8
#define OLED_VCC 9
#define OLED_GND 10

#define SAMPLE_DURATION 1*1000 // 1 second
#define LOG_DURATION 5*60*1000 // 5 mins

// Instantiate Modules
DisplayManager display(U8G2_R0, /* reset=*/U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
TemperatureManager tempMgr(SENSORS_PIN);
ThermalController thermalCtrl(PELTIER_INT1, PELTIER_INT2, FAN_PWM);
WebManager webMgr(&tempMgr, &thermalCtrl);

// Initialize Classes
unsigned long lastHB = 0;
unsigned long lastLog = 0;

void setup()
{
    Serial.begin(115200);
    delay(500);

    // 1. Filesystem First
    if (!FileSystem::begin())
        return;

    // 2. Hardware
    // Setup Virtual Power Pins for OLED
    pinMode(OLED_GND, OUTPUT);
    digitalWrite(OLED_GND, LOW); // 0V
    pinMode(OLED_VCC, OUTPUT);
    digitalWrite(OLED_VCC, HIGH); // 3.3V
    delay(500);                   // Give OLED/Sensor time to wake up

    display.begin();
    tempMgr.begin();
    thermalCtrl.begin(20.0); // Default setpoint

    // 3. Network
    display.printStr(0, 12, "Connecting to WiFi...");
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Add this after WiFi.begin()
    WiFi.setSleep(true);                // Enables WiFi Modem-sleep to save power
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Lowers transmit power (Standard is 20dBm)

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000)
    {
        Serial.print(".");
        delay(500);
        // This prevents the hardware watchdog from triggering during the wait
        yield();
    }

    // 3. Network & API
    display.printStr(0, 12, "Starting Web Server...");
    webMgr.begin();
}

static String currentIP = "Connecting...";
static wl_status_t lastWiFiStatus = WL_IDLE_STATUS;
void loop()
{
    unsigned long now = millis();

    tempMgr.updateDiscovery();

    // Control Loop (PID & Display)
    if (now - lastHB >= SAMPLE_DURATION)
    {
        lastHB = now;
        Temperatures t = tempMgr.readStored();
        tempMgr.addSample(t);

        wl_status_t currentWiFiStatus = WiFi.status();
        if (currentWiFiStatus != lastWiFiStatus) {
            if (currentWiFiStatus == WL_CONNECTED) {
                currentIP = WiFi.localIP().toString();
            } else {
                currentIP = "No WiFi";
            }
            lastWiFiStatus = currentWiFiStatus;
        }

        thermalCtrl.updatePID(t.blockTemp, t.ambientTemp);
        display.update(t.blockTemp, t.ambientTemp, true, currentIP.c_str());

        tempMgr.requestNewScan();
    }

    // Logging Loop
    if (now - lastLog >= LOG_DURATION)
    {
        lastLog = now;
        webMgr.logDataToFS(tempMgr.readStored());
    }
}