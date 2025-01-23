/*******************************************************************
    A telegram bot for your ESP32 that controls the 
    onboard LED. The LED in this example is active low.

    Parts:
    ESP32 D1 Mini stlye Dev board* - http://s.click.aliexpress.com/e/C6ds4my
    (or any ESP32 board)

      = Affilate

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/


    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/
#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "sensorReadings.h"
#include "tokens.h"

#define PRINT_ADDRESS_DS18B20

#define HEAT_PIN (21)
#define COOL_PIN (22)
#define FAN_PIN  (23)

#define MODE_OFF  (0)
#define MODE_AUTO (1)
#define MODE_HEAT (2)
#define MODE_COOL (3)
#define UNDEFINED (0)
#define HEATING   (1)
#define COOLING   (2)
#define COOL_WAIT (120000)
#define FAN_WAIT  (300000)

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

const int ledPin = LED_BUILTIN;
int ledStatus = 0;
float refTemp, tempH = 22.0, tempHH = 23.0, tempL = 18.0, tempLL = 17.0;
UBaseType_t selectedMode = MODE_AUTO;
UBaseType_t currentMode  = UNDEFINED;
uint8_t chamberAdd[] = DS18B20_CHAMBER;
uint8_t liquidAdd[]  = DS18B20_LIQUID;
SemaphoreHandle_t xReadTempSemaphore;

bool coolingState = false;
bool heatingState = false;
bool blowingState = false;
bool canRestart   = false;
bool canStopFan   = false;

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/status")
    {
      String statusString;
      String sMode;
      String sOperating;
      String sFan;
      String sCooler;
      String sHeater;
      switch (selectedMode)
      {
        case MODE_OFF :
          sMode = "Apagado";
          break;
        case MODE_AUTO :
          sMode = "Automatico";
          break;
        case MODE_COOL :
          sMode = "Enfriamiento";
          break;
        case MODE_HEAT :
          sMode = "Calentamiento";
          break;
        default:
          sMode = "No reconocido";
      }
      switch (currentMode)
      {
        case UNDEFINED :
          sOperating = "Sin definir";
          break;
        case HEATING :
          sOperating = "Calentamiento";
          break;
        case COOLING :
          sOperating = "Enfriamiento";
          break;
        default:
          sOperating = "No reconocido";
      }
      sFan    = blowingState ? "Encendido" : "Apagado";
      sCooler = coolingState ? "Encendido" : "Apagado";
      sHeater = heatingState ? "Encendido" : "Apagado";
      if (xSemaphoreTake(xReadTempSemaphore, portMAX_DELAY) == pdTRUE)
      {
        statusString = "Modo de operación seleccionado: " + sMode + "\n" +
                      "Modo de operación en funcionamiento: " + sOperating + "\n" +
                      "Ventilador: " + sFan + "\n" +
                      "Enfriador: " + sCooler + "\n" +
                      "Calentador: " + sHeater + "\n" +
                      "Temperatura en la camara: " + readDSTempStringCByAdd(chamberAdd) + "°C\n" +
                      "Temperatura en el liquido: " + readDSTempStringCByAdd(liquidAdd) + "°C\n" +
                      "Temperatura superior de histéresis: " + String(tempH) + "°C\n" +
                      "Temperatura inferior de histéresis: " + String(tempL) + "°C\n" +
                      "Temperatura superior de cambio de modo: " + String(tempHH) + "°C\n" +
                      "Temperatura inferior de cambio de modo: " + String(tempLL) + "°C\n";
        xSemaphoreGive(xReadTempSemaphore);
      }
      else
      {
        statusString = "No se obtuvo el semaforo";
      }
      bot.sendMessage(chat_id, statusString, "Markdown");
    }

    if (text == "/getTemp")
    {
      if (xSemaphoreTake(xReadTempSemaphore, portMAX_DELAY) == pdTRUE)
      {
        String tempString = "Temperatura en la camara: " + readDSTempStringCByAdd(chamberAdd) + "°C\n" +
                            "Temperatura en el liquido: " + readDSTempStringCByAdd(liquidAdd) + "°C\n";
        xSemaphoreGive(xReadTempSemaphore);
        bot.sendMessage(chat_id, tempString, "Markdown");
      }
    }

    if (text == "/getChamberTemp")
    {
      if (xSemaphoreTake(xReadTempSemaphore, portMAX_DELAY) == pdTRUE)
      {
        String tempString = "Temperatura en la camara: " + readDSTempStringCByAdd(chamberAdd) + "°C\n";
        xSemaphoreGive(xReadTempSemaphore);
        bot.sendMessage(chat_id, tempString, "Markdown");
      }
    }

    if (text == "/getLiquidTemp")
    {
      if (xSemaphoreTake(xReadTempSemaphore, portMAX_DELAY) == pdTRUE)
      {
        String tempString = "Temperatura en el liquido: " + readDSTempStringCByAdd(liquidAdd) + "°C\n";
        xSemaphoreGive(xReadTempSemaphore);
        bot.sendMessage(chat_id, tempString, "Markdown");
      }
    }

    if (text == "/setModeOff")  selectedMode = MODE_OFF;
    if (text == "/setModeAuto") selectedMode = MODE_AUTO;
    if (text == "/setModeCool") selectedMode = MODE_COOL;
    if (text == "/setModeHeat") selectedMode = MODE_HEAT;

    if (text == "/start")
    {
      String welcome = "Hola, " + from_name + ".\n";
      welcome += "Por acá se interactúa con el control de procesos de cerveza casera.\n\n";
      welcome += "/setModeAuto : para que enfríe o caliente según haga falta\n";
      welcome += "/setModeCool : modo solo enfriamiento\n";
      welcome += "/setModeHeat : modo solo calentamiento\n";
      welcome += "/setModeOff : modo apagado\n";
      welcome += "/getTemp : lista todas las temperaturas leídas\n";
      welcome += "/getChamberTemp : Temperatura en la cámara\n";
      welcome += "/getLiquidTemp : Temperatura en el líquido\n";
      welcome += "/status : Estado general del sistema.\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

/** tareas ********************************************/
/* check for new messages */
/* TODO make this task to execute with freeRTOS timer*/
void vCheckNewMessagesTask(void *px)
{
  /* last time messages' scan has been done */
  static unsigned long bot_lasttime = millis();

  while(1)
  {
    if (millis() - bot_lasttime > BOT_MTBS)
    {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages)
      {
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      bot_lasttime = millis();
    }
  }

  /* Must not exit, but if you leave the while(1) you can delete the task */
  vTaskDelete(NULL);
}

    
/* check for temperature bounds - ¿log? */

/* Temperature control */
void vTempControl(void* px)
{
  TickType_t xTimeOff = xTaskGetTickCount();
  TickType_t xTimeCur;
  while(1){
    xTimeCur = xTaskGetTickCount();
    if (xTimeCur < xTimeOff) xTimeOff = xTimeCur;
    canRestart = xTimeCur - xTimeOff > COOL_WAIT ? true : false;
    canStopFan = xTimeCur - xTimeOff > FAN_WAIT  ? true : false;
    if (xSemaphoreTake(xReadTempSemaphore, 15) == pdTRUE)
    {
      refTemp = readDSTempC(chamberAdd);
      xSemaphoreGive(xReadTempSemaphore);
    }
    if (selectedMode != MODE_OFF)
    {
      /* mode changes */
      switch (selectedMode)
      {
        case MODE_AUTO :
          switch (currentMode)
          {
            case UNDEFINED :
              if (refTemp > tempHH) currentMode = COOLING;
              else if (refTemp < tempLL) currentMode = HEATING;
              break;
            case HEATING :
              if (refTemp > tempHH) currentMode = COOLING;
              break;
            case COOLING :
              if (refTemp < tempLL) currentMode = HEATING;
              break;
            default:
              currentMode = UNDEFINED;
          }
          break;
        case MODE_COOL :
          currentMode = COOLING;
          break;
        case MODE_HEAT :
          currentMode = HEATING;
          break;
        default:
          currentMode  = UNDEFINED;
          selectedMode = MODE_OFF;
      }
      /* temp control */
      switch (currentMode)
      {
        case UNDEFINED :
          coolingState = false;
          heatingState = false;
          if (blowingState && canStopFan)
          {
            blowingState = false;
          }
          break;
        case COOLING :
          if (heatingState)
          {
            xTimeOff = xTaskGetTickCount();
            heatingState = false;
          }
          if (coolingState && (refTemp < tempL)) 
          {
            
            coolingState = false;
          } 
          else if (!(coolingState))
          {
            if (canRestart && (refTemp > tempH))
            {
              coolingState = true;
              blowingState = true;
            }
            else if (blowingState && canStopFan)
            {
              blowingState = false;
            }
          }
          break;
        case HEATING :
          if (coolingState)
          {
            xTimeOff = xTaskGetTickCount();
            coolingState = false;
          }
          if (heatingState && (refTemp > tempH)) 
          {
            xTimeOff = xTaskGetTickCount();
            heatingState = false;
          } 
          else if (!(heatingState))
          {
            if (refTemp < tempL)
            {
              heatingState = true;
              blowingState = true;
            }
            else if (blowingState && canStopFan)
            {
              blowingState = false;
            }
          }
          break;
        default:
          currentMode = UNDEFINED;
      }
      blowingState ? digitalWrite(FAN_PIN, HIGH)  : digitalWrite(FAN_PIN,  LOW);
      coolingState ? digitalWrite(COOL_PIN, HIGH) : digitalWrite(COOL_PIN, LOW);
      heatingState ? digitalWrite(HEAT_PIN, HIGH) : digitalWrite(HEAT_PIN, LOW);
    } else {
      currentMode = UNDEFINED;
      if (digitalRead(COOL_PIN))
      {
        xTimeOff = xTaskGetTickCount();
      }
      digitalWrite(COOL_PIN, LOW);
      digitalWrite(HEAT_PIN, LOW);
      digitalWrite(FAN_PIN,  LOW);
      coolingState = false;
      heatingState = false;
      blowingState = false;
    }
    vTaskDelay(500);
  }
  /* Must not exit, but if you leave the while(1) you can delete the task */
  vTaskDelete(NULL);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(COOL_PIN, OUTPUT);
  delay(10);
  digitalWrite(ledPin, LOW); // initialize pin as off (active LOW)
  digitalWrite(HEAT_PIN, LOW);
  digitalWrite(COOL_PIN, LOW);

  setupSensorsOnOneWire();

  #ifdef PRINT_ADDRESS_DS18B20
    DeviceAddress tempDeviceAddress;

    int numberOfDevices = getSensorCount();
    
    // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    // Loop through each device, print out address
    for(int i=0;i<numberOfDevices; i++) {
      // Search the wire for address
      if(getAddress(tempDeviceAddress, i)) {
        Serial.print("Found device ");
        Serial.print(i, DEC);
        Serial.print(" with address: ");
      
        for (int j=0;j<8;j++)
        {
          Serial.printf("%02X ", tempDeviceAddress[j]);
        }
        Serial.println();
      } else {
        Serial.print("Found ghost device at ");
        Serial.print(i, DEC);
        Serial.print(" but could not detect address. Check power and cabling");
      }
    }
  #endif /* PRINT_ADDRESS_DS18B20 */

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  
  Serial.println(now);

  xReadTempSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xReadTempSemaphore);

  xTaskCreate(vCheckNewMessagesTask, "checkMsg", 0x2000, NULL, 2, NULL);
  xTaskCreate(vTempControl, "tempControl", 0x2000, NULL, 2, NULL);
}

void loop()
{
  
}
