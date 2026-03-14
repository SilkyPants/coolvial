#include "ThermalController.h"

#include <Preferences.h>

ThermalController::ThermalController(uint8_t pIn1, uint8_t pIn2, uint8_t pFan) 
    : pinIn1(pIn1), pinIn2(pIn2), pinFan(pFan) {
    peltierPID = new PID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);
}

void ThermalController::begin(double target) {
    setpoint = target;
    pinMode(pinIn1, OUTPUT);
    pinMode(pinIn2, OUTPUT);
    pinMode(pinFan, OUTPUT);
    
    peltierPID->SetMode(AUTOMATIC);
    peltierPID->SetOutputLimits(-255, 255); // Negative for Heating, Positive for Cooling

    loadConfig();
}

void ThermalController::updatePID(float currentBlock, float currentAmbient) {
    input = (double)currentBlock;
    
    // Safety: If ambient is too high, limit cooling power to prevent thermal runaway
    if (currentAmbient > 35.0) {
        peltierPID->SetOutputLimits(-180, 180);
    } else {
        peltierPID->SetOutputLimits(-255, 255);
    }

    peltierPID->Compute();
    drivePeltier(output);
}

void ThermalController::drivePeltier(double pwm) {
    if (pwm > 0) { // Cooling Mode
        analogWrite(pinIn1, (int)pwm);
        digitalWrite(pinIn2, LOW);
    } else { // Heating Mode
        digitalWrite(pinIn1, LOW);
        analogWrite(pinIn2, (int)abs(pwm));
    }
}

void ThermalController::setSetpoint(double sp) {
    setpoint = sp;
}

void ThermalController::updateTunings(double p, double i, double d) {
    Kp = p; Ki = i; Kd = d;
    peltierPID->SetTunings(Kp, Ki, Kd);
}

void ThermalController::loadConfig() {
    Preferences prefs;
    prefs.begin("coolvial-conf", true); // Read-only mode
    
    // Load with defaults if keys don't exist
    setpoint = prefs.getFloat("setpoint", 20.0f);
    Kp = prefs.getFloat("kp", 30.0f);
    Ki = prefs.getFloat("ki", 0.5f);
    Kd = prefs.getFloat("kd", 15.0f);
    
    prefs.end();
    
    // Apply the loaded values to the active PID object
    peltierPID->SetTunings(Kp, Ki, Kd);
}