#ifndef SENSOR_READINGS_H
#define SENSOR_READINGS_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include "tokens.h"

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 4

class FridgeTemps
{
private:
    float refTemp, chamberTemp, liquidTemp;
    uint8_t chamberAdd[8] = DS18B20_CHAMBER;
    uint8_t liquidAdd[8]  = DS18B20_LIQUID;
    DallasTemperature sensors;
public:
    FridgeTemps();
    void updateTemp();
    void setChamberAdd(uint8_t*);
    void setLiquidAdd(uint8_t*);
    float getChamberTemp();
    float getLiquidTemp();
    float getRefTemp();
    int getSensorCount();
    bool getAddress(uint8_t*, int);
};

#endif /* !SENSOR_READINGS_H */