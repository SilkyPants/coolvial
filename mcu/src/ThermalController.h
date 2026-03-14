#pragma once

#include <Arduino.h>

#include <PID_v1.h>

class ThermalController {
private:
    double setpoint, input, output;
    double Kp, Ki, Kd;
    PID* peltierPID;
    
    uint8_t pinIn1, pinIn2, pinFan;

public:
    ThermalController(uint8_t pIn1, uint8_t pIn2, uint8_t pFan);
    void begin(double targetTemp);
    
    void updatePID(float currentBlockTemp, float currentAmbientTemp);
    void drivePeltier(double pwm);
    void setFanSpeed(uint8_t speedPercent);

    // Getters for the Web API
    double getSetpoint() { return setpoint; }
    double getKp() { return Kp; }
    double getKi() { return Ki; }
    double getKd() { return Kd; }

    // Setters that update the live PID object
    void setSetpoint(double sp);
    void updateTunings(double p, double i, double d);
    void loadConfig();
};