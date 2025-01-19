#ifndef SENSOR_READINGS_H
#define SENSOR_READINGS_H

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 4

/* Init one wire for ds18b20 */
void setupSensorsOnOneWire();

/* Read a sensor by index. float format */
float readDSTempC(uint8_t);

/* Read a sensor by address. float format */
float readDSTempC(uint8_t*);

/* Call sensors.requestTemperatures() to issue a global temperature 
 * and Requests to all devices on the bus
 */
String readDSTempStringC(uint8_t);

/* Call sensors.requestTemperatures(address)
 */
String readDSTempStringCByAdd(uint8_t*);

/* Call sensors.getDeviceCount() */
int getSensorCount();

/* Call sensors.getAddress */
bool getAddress(DeviceAddress, int);

#endif /* !SENSOR_READINGS_H */