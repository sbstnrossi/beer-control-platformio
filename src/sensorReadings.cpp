#include "sensorReadings.h"

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

/* Init one wire for ds18b20 */
void setupSensorsOnOneWire()
{
  // Start up the DS18B20 library
  sensors.begin();
}

/* Read a sensor by index. float format */
float readDSTempC(uint8_t sensorIndex)
{
  float tempC;
  sensors.requestTemperatures(); 
  tempC = sensors.getTempCByIndex(sensorIndex);
  return tempC;  
}

/* Read a sensor by address. float format */
float readDSTempC(uint8_t* add)
{
  float tempC;
  sensors.requestTemperaturesByAddress(add); 
  tempC = sensors.getTempC(add);
  return tempC; 
}

/* Call sensors.requestTemperatures() to issue a global temperature 
 * and Requests to all devices on the bus
 */
String readDSTempStringC(uint8_t sensorIndex)
{
  float tempC;
  String tempCString;

  tempC = readDSTempC(sensorIndex);
  if(tempC == -127.00) {
    Serial.println("Failed to read from DS18B20 sensor");
    tempCString = "--";
  } else {
    Serial.print("Temperature Celsius: ");
    Serial.println(tempC); 
    tempCString = String(tempC);
  }
  return tempCString;
}

String readDSTempStringCByAdd(uint8_t* sensorAdd)
{
  float tempC;
  String tempCString;

  tempC = readDSTempC(sensorAdd);
  if(tempC == -127.00) {
    Serial.println("Failed to read from DS18B20 sensor");
    tempCString = "--";
  } else {
    Serial.print("Temperature Celsius: ");
    Serial.println(tempC); 
    tempCString = String(tempC);
  }
  return tempCString;
}

int getSensorCount()
{
  return sensors.getDeviceCount();
}

bool getAddress(DeviceAddress addr, int i)
{
  return sensors.getAddress(addr, i);
}