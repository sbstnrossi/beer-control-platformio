#include "sensorReadings.h"

FridgeTemps::FridgeTemps()
{
  // Setup a oneWire instance to communicate with any OneWire devices
  OneWire oneWire(ONE_WIRE_BUS);
  // Pass our oneWire reference to Dallas Temperature sensor 
  DallasTemperature sensors(&oneWire);
  sensors.begin();
}

void FridgeTemps::updateTemp()
{
  sensors.requestTemperaturesByAddress(chamberAdd);
  chamberTemp = sensors.getTempC(chamberAdd);
  sensors.requestTemperaturesByAddress(liquidAdd); 
  liquidTemp = sensors.getTempC(liquidAdd);
}

void FridgeTemps::setChamberAdd(uint8_t* pAdd)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    chamberAdd[i] = pAdd[i];
  }
}

void FridgeTemps::setLiquidAdd(uint8_t* pAdd)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    liquidAdd[i] = pAdd[i];
  }
}

float FridgeTemps::getChamberTemp()
{
  return chamberTemp; 
}

float FridgeTemps::getLiquidTemp()
{
  return liquidTemp; 
}

float FridgeTemps::getRefTemp()
{
  return chamberTemp;
}

int FridgeTemps::getSensorCount()
{
  return sensors.getDeviceCount();
}

bool FridgeTemps::getAddress(uint8_t* addr, int i)
{
  return sensors.getAddress(addr, i);
}