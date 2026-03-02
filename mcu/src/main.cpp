#include <Arduino.h>

#include <U8g2lib.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// Pins for DS18B20 bus
#define SENSORS_PIN 4

#define FAN_TACH 3
#define FAN_SPEED_PWM 2

#define PELTIER_INT1 1
#define PELTIER_INT2 0

#define SCREEN_PIN 11

// Pins for OLED
#define OLED_SDA 7
#define OLED_SCL 8
#define OLED_VCC 9
#define OLED_GND 10

fs::LittleFSFS StaticFS = fs::LittleFSFS();
fs::LittleFSFS StorageFS = fs::LittleFSFS();

#define MOUNT_NAME_STATIC "static"
#define MOUNT_NAME_USER "storage"
#define MOUNT_STATIC "/" MOUNT_NAME_STATIC
#define MOUNT_USER "/" MOUNT_NAME_USER

#define FILE_HOME_SCRIPT "/script.js"
#define FILE_HOME_STYLE "/style.css"
#define FILE_HOME_INDEX "/index.html"
#define FILE_LOGS "/logs.csv"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
WebServer server(80);

const int SAMPLE_INTERVAL = 2000;       // 2 seconds
const int LOG_INTERVAL = 5 * 60 * 1000; // 5 minutes
const int BUFFER_SIZE = 50;             // Number of samples to average

float tempBuffer[BUFFER_SIZE];
int bufferIndex = 0;
unsigned long lastSampleTime = 0;
unsigned long lastLogTime = 0;

Temperatures _lastTemps;
float lastLoggedValue = 0;
unsigned long lastLoggedTime = 0;

// Setup DS18B20 bus
OneWire oneWire(SENSORS_PIN);
DallasTemperature sensors(&oneWire);

// Setup OLED (SSD1306/SSD1315 use the same driver in U8g2)
// F designates Full Buffer mode for smoother graphics
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

struct Temperatures
{
  float blockTemp = 0;
  float internalTemp = 0;
  float ambientTemp = 0;
};

void addSample(float val);
float getAverage();
void logToFS(float avgTemp);
Temperatures readProbes();

void setupRouting();
void handleDataAPI();

void setup()
{

  delay(100); // Give the OLED a moment to wake up

  // 2. Start Serial
  Serial.begin(115200);
  Serial.println("\n--- BOOTING ---");
  // while (!Serial && millis() < 5000)
  //   ;

  pinMode(SCREEN_PIN, INPUT_PULLUP);

  // 1. Setup Virtual Power Pins for OLED
  pinMode(OLED_GND, OUTPUT);
  digitalWrite(OLED_GND, LOW); // 0V
  pinMode(OLED_VCC, OUTPUT);
  digitalWrite(OLED_VCC, HIGH); // 3.3V
  delay(500);                   // Give OLED/Sensor time to wake up
  u8g2.begin();

  u8g2.firstPage();
  do
  {
    u8g2.setFont(u8g2_font_ncenB08_tf); // Choose a nice font
    u8g2.drawUTF8(0, 12, "Connecting to WiFi");
  } while (u8g2.nextPage());

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

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("\nWiFi Failed (Will retry in loop)");
  }

  timeClient.begin();
  timeClient.update();

  // 3. Start Sensors & Display
  sensors.begin();
  sensors.setWaitForConversion(false); // Non-blocking

  // 3. Mount LittleFS (This can take a long time if formatting!)
  Serial.println("Mounting LittleFS...");
  if (!StaticFS.begin(false, MOUNT_STATIC, 5, MOUNT_NAME_STATIC))
  {
    Serial.println("LittleFS Static Data Mount Failed");
  }
  else
  {
    Serial.println("LittleFS Static Data Mounted Successfully");
  }

  delay(500); // Give OLED/Sensor time to wake up

  if (!StorageFS.begin(true, MOUNT_USER, 5, MOUNT_NAME_USER))
  {
    Serial.println("LittleFS Persistent Mount Failed");
  }
  else
  {
    Serial.println("LittleFS Persistent Mounted Successfully");
  }

  setupRouting();

  server.begin();
  Serial.println("HTTP Server Started");

  sensors.requestTemperatures();
  delay(750); // Wait for first conversion
  float initialTemp = readProbes().blockTemp;
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    tempBuffer[i] = initialTemp;
  }
  
  logToFS(getAverage());

  Serial.println("System Initialized");
}

int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 100;
bool isSleeping = false;
void checkScreenBtn()
{
  int reading = digitalRead(SCREEN_PIN);

  // Check if the button state changed (due to noise or pressing)
  if (reading != lastButtonState)
  {
    lastDebounceTime = millis(); // Reset the debouncing timer
    lastButtonState = reading;
  }

  // If the state has stayed the same for longer than our delay...
  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    // If the button state has actually changed:
    if (reading == LOW)
    {
      // This block only runs ONCE per press
      Serial.println("Debounced Press Detected!");
      isSleeping = !isSleeping;
      u8g2.setPowerSave(isSleeping);
    }
  }
}

void loop()
{
  server.handleClient();
  timeClient.update();

  unsigned long currentMillis = millis();

  bool updateDisplay = false;
  // 1. High-frequency Sampling (Every 2 seconds)
  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL)
  {
    _lastTemps = readProbes();
    addSample(_lastTemps);
    lastLoggedValue = _lastTemps.blockTemp;
    lastSampleTime = currentMillis;
    updateDisplay = true;
  }

  // 2. Low-frequency Logging (Every X minutes)
  if (currentMillis - lastLogTime >= LOG_INTERVAL)
  {
    float average = getAverage();
    logToFS(average);
    lastLogTime = currentMillis;
    updateDisplay = true;
  }

  checkScreenBtn();

  if (!isSleeping && updateDisplay)
  {
    // Update Serial
    Serial.print("Temp: ");
    Serial.println(_lastTemps.blockTemp);
    Serial.print("Avg: ");
    Serial.println(getAverage());

    u8g2.firstPage();
    do
    {
      // 1. Average Temp Line (Top)
      char avgBufferStr[16];
      sprintf(avgBufferStr, "Avg Temp: %.1f °C", getAverage());
      u8g2.setFont(u8g2_font_ncenB08_tf); // Choose a nice font
      u8g2.drawUTF8(0, 12, avgBufferStr);

      // 2. Main Current Temp (Middle)
      if (_lastTemps.blockTemp != DEVICE_DISCONNECTED_C)
      {
        char str[10];
        dtostrf(_lastTemps.blockTemp, 4, 1, str); // Using 1 decimal place to prevent overlap

        u8g2.setFont(u8g2_font_logisoso24_tf);
        u8g2.drawStr(0, 48, str);

        // Degree symbol aligned with the large text
        u8g2.setFont(u8g2_font_ncenB08_tf); // Switch back to smaller font for the symbol
        u8g2.drawUTF8(75, 48, "°C");
      }
      else
      {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 40, "Sensor Error!");
      }

      // 3. IP Address Footer (Bottom)
      u8g2.setFont(u8g2_font_6x10_tf);
      if (WiFi.status() == WL_CONNECTED)
      {
        // Using .c_str() directly to save memory
        u8g2.drawStr(0, 64, WiFi.localIP().toString().c_str());
      }
      else
      {
        u8g2.drawStr(0, 64, "WiFi: Connecting...");
      }

    } while (u8g2.nextPage());
  }

  delay(2);
}

void addSample(float val)
{
  tempBuffer[bufferIndex] = val;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE; // Circular wrap-around
}

float getAverage()
{
  float sum = 0;
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    sum += tempBuffer[i];
  }
  return sum / BUFFER_SIZE;
}

Temperatures readProbes()
{
  Temperatures temps;
  sensors.requestTemperatures();

  temps.blockTemp = sensors.getTempCByIndex(0);

  return temps;
}

void logToFS(float avgTemp)
{
  File file = StorageFS.open(FILE_LOGS, FILE_APPEND, true);
  if (file)
  {
    // 1. Get the current Epoch time from NTP
    unsigned long now = timeClient.getEpochTime();
    lastLoggedTime = now;

    // 2. Save: Timestamp, Temperature
    file.print(now);
    file.print(",");
    file.println(avgTemp);

    file.close();
    Serial.printf("Logged to FS: %lu, %.2f\n", now, avgTemp);
  }
  else
  {
    Serial.println("Failed to open file for logging!");
  }
}

void setupRouting()
{
  // Static File Serving
  server.on("/", HTTP_GET, []()
            {
        File file = StaticFS.open(FILE_HOME_INDEX, "r");
        server.streamFile(file, "text/html");
        file.close(); });

  server.on("/style.css", HTTP_GET, []()
            {
        File file = StaticFS.open(FILE_HOME_STYLE, "r");
        server.streamFile(file, "text/css");
        file.close(); });

  server.on("/script.js", HTTP_GET, []()
            {
        File file = StaticFS.open(FILE_HOME_SCRIPT, "r");
        server.streamFile(file, "application/javascript");
        file.close(); });

  // API Routes
  server.on("/api/data", HTTP_GET, handleDataAPI);
  server.on("/api/logs", HTTP_GET, []()
            {
        File file = StorageFS.open(FILE_LOGS, "r");
        server.streamFile(file, "text/csv");
        file.close(); });
}

void handleDataAPI()
{
  JsonDocument doc;
  doc["lastTemp"] = _lastTemps.blockTemp;
  doc["avgTemp"] = getAverage();
  doc["timestamp"] = lastLoggedTime; // The timestamp of the ACTUAL last save
  doc["logVal"] = lastLoggedValue;   // The value that was actually saved to CSV

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}
