#include "TemperatureManager.h"

#define BLOCK_PROBE_KEY "addr_blk"
#define INTERNAL_PROBE_KEY "addr_int"
#define AMBIENT_PROBE_KEY "addr_amb"

const char *keys[] = {
    BLOCK_PROBE_KEY,
    INTERNAL_PROBE_KEY,
    AMBIENT_PROBE_KEY};

TemperatureManager::TemperatureManager(uint8_t pin)
{
    oneWire = new OneWire(pin);
    sensors = new DallasTemperature(oneWire);
}

void TemperatureManager::begin()
{
    sensors->begin();
    prefs.begin("coolvial-conf", false);
    loadAddressesFromNVS();


    Serial.printf("%d devices connected, and %d DS18.\n", sensors->getDeviceCount(), sensors->getDS18Count());

    // Initial priming of the buffer
    Temperatures initial = readInstant();
    for (int i = 0; i < BUFFER_SIZE; i++)
        buffer[i] = initial;
}

void TemperatureManager::loadAddressesFromNVS() {
    _roleAssigned[0] = prefs.getBytes(BLOCK_PROBE_KEY, blockAddr, 8) == 8;
    _roleAssigned[1] = prefs.getBytes(INTERNAL_PROBE_KEY, internalAddr, 8) == 8;
    _roleAssigned[2] = prefs.getBytes(AMBIENT_PROBE_KEY, ambientAddr, 8) == 8;
}

void TemperatureManager::assignSensorToRole(DeviceAddress addr, Role role) {
    int idx = (int)role;

    // 1. Save to Flash (NVS)
    prefs.putBytes(keys[idx], addr, 8);
    _roleAssigned[idx] = true;

    // 2. Update RAM immediately so the PID loop sees the sensor
    if (role == Role::BLOCK) memcpy(blockAddr, addr, 8);
    else if (role == Role::INTERNAL) memcpy(internalAddr, addr, 8);
    else if (role == Role::AMBIENT) memcpy(ambientAddr, addr, 8);
    
    Serial.printf("Role %d assigned and active.\n", idx);
}

Temperatures TemperatureManager::readInstant()
{
    sensors->requestTemperatures();

    Temperatures t;
    t.blockTemp = sensors->getTempC(blockAddr);
    t.internalTemp = sensors->getTempC(internalAddr);
    t.ambientTemp = sensors->getTempC(ambientAddr);

    // Fallback: If address not found, use index (for initial setup)
    if (t.blockTemp == DEVICE_DISCONNECTED_C)
    {
        t.blockTemp = sensors->getTempCByIndex(0);
    }

    return t;
}

void TemperatureManager::addSample(Temperatures t)
{
    buffer[bufferIndex] = t;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
    if (samplesTaken < BUFFER_SIZE)
    {
        samplesTaken++;
    }
}

Temperatures TemperatureManager::getRollingAverage()
{
    Temperatures avg;
    for (int i = 0; i < samplesTaken; i++)
    {
        avg.blockTemp += buffer[i].blockTemp;
        avg.internalTemp += buffer[i].internalTemp;
        avg.ambientTemp += buffer[i].ambientTemp;
    }
    avg.blockTemp /= samplesTaken;
    avg.internalTemp /= samplesTaken;
    avg.ambientTemp /= samplesTaken;
    return avg;
}

void TemperatureManager::startDiscovery(Role role)
{
    _activeSearch = role;
    _searchStartTime = millis();

    // Capture baseline for whatever probes are currently plugged in (max 3)
    sensors->requestTemperatures();
    for (int i = 0; i < 3; i++)
    {
        _baselines[i] = sensors->getTempCByIndex(i);
    }

    _state = DiscoveryState::SEARCHING;
}

void TemperatureManager::updateDiscovery()
{
    if (_state != DiscoveryState::SEARCHING)
        return;

    if (_activeSearch == Role::NONE)
    {
        _state = DiscoveryState::FAILED;
        return;
    }
    if (millis() - _searchStartTime > 15000)
    {
        _state = DiscoveryState::FAILED;
        _activeSearch = Role::NONE;
        return;
    }

    sensors->requestTemperatures();
    for (int i = 0; i < 3; i++)
    {
        float current = sensors->getTempCByIndex(i);

        // If this probe spiked > 3°C from its own baseline
        if (current - _baselines[i] > 3.0f)
        {
            DeviceAddress foundAddr;
            sensors->getAddress(foundAddr, i);

            assignSensorToRole(foundAddr, _activeSearch);

            _activeSearch = Role::NONE;
            _state = DiscoveryState::SUCCESS;
        }
    }
}