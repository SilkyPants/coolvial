#pragma once
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

struct Temperatures
{
    float blockTemp = 0;
    float internalTemp = 0;
    float ambientTemp = 0;
};

// Specializing Converter to teach ArduinoJson how to handle 'Temperatures'
namespace ArduinoJson {
    template <>
    struct Converter<Temperatures> {
        // Defines how to convert YOUR struct TO JSON
        static void toJson(const Temperatures& src, JsonVariant dst) {
            JsonObject obj = dst.to<JsonObject>();
            obj["block"] = src.blockTemp;
            obj["internal"] = src.internalTemp;
            obj["ambient"] = src.ambientTemp;
        }

        // Defines how to convert FROM JSON back into YOUR struct
        static Temperatures fromJson(JsonVariantConst src) {
            Temperatures dst;
            dst.blockTemp = src["block"] | 0.0f;
            dst.internalTemp = src["internal"] | 0.0f;
            dst.ambientTemp = src["ambient"] | 0.0f;
            return dst;
        }

        // Checks if the JSON matches your struct (required for is<Temperatures>())
        static bool checkJson(JsonVariantConst src) {
            return src.is<JsonObject>();
        }
    };
}

enum class Role { BLOCK = 0, INTERNAL = 1, AMBIENT = 2, NONE = 3 };
enum class DiscoveryState { IDLE, SEARCHING, SUCCESS, FAILED };

class TemperatureManager
{
private:
    OneWire oneWire;
    DallasTemperature sensors;
    Preferences prefs;

    DeviceAddress blockAddr, internalAddr, ambientAddr;
    bool _roleAssigned[3];        // Tracking if we have an address for this role
    
    // Discovery State Machine
    DiscoveryState _state = DiscoveryState::IDLE;
    Role _activeSearch = Role::NONE;
    unsigned long _searchStartTime;
    float _baselines[3];    // Initial temps of the 3 physical probes

    static const int BUFFER_SIZE = 50;
    Temperatures buffer[BUFFER_SIZE];
    int bufferIndex = 0;
    int samplesTaken = 0; // Fixes the average bug

public:
    TemperatureManager(uint8_t pin);
    void begin();
    void loadAddressesFromNVS();
    void assignSensorToRole(DeviceAddress addr, Role role);

    // Discovery API
    void startDiscovery(Role role);
    void updateDiscovery();
    DiscoveryState getDiscoveryState() { return _state; }

    Temperatures readStored();
    void requestNewScan() { sensors.requestTemperatures(); };
    void addSample(Temperatures t);
    Temperatures getRollingAverage();
};