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
#include "parameters.h"
#include "tokens.h"

#define PRINT_ADDRESS_DS18B20

#define HEAT_PIN (25)
#define COOL_PIN (26)
#define FAN_PIN  (27)

#define MODE_OFF  (0)
#define MODE_AUTO (1)
#define MODE_HEAT (2)
#define MODE_COOL (3)
#define UNDEFINED (0)
#define HEATING   (1)
#define COOLING   (2)

#define COOL_WAIT (120000)
#define FAN_WAIT  (300000)

#define READ_PERIOD    (250)
#define CONTROL_PERIOD (500)
#define BOT_PERIOD    (1000)

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

struct TaskParameters 
{
  TempParameters tempParams;
  FridgeTemps    tempReadings;
};

UBaseType_t selectedMode = MODE_AUTO;
UBaseType_t currentMode  = UNDEFINED;

bool coolingState = false;
bool heatingState = false;
bool blowingState = false;
bool canRestart   = false;
bool canStopFan   = false;

void handleNewMessages(int numNewMessages, void* params)
{
  TaskParameters pParams = *(struct TaskParameters*)params;
  TempParameters tempParams   = pParams.tempParams;
  FridgeTemps    tempReadings = pParams.tempReadings;
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
      statusString = "Modo de operación seleccionado: " + sMode + "\n" +
                    "Modo de operación en funcionamiento: " + sOperating + "\n" +
                    "Ventilador: " + sFan + "\n" +
                    "Enfriador: " + sCooler + "\n" +
                    "Calentador: " + sHeater + "\n" +
                    "Temperatura en la camara: " + String(tempReadings.getChamberTemp()) + "°C\n" +
                    "Temperatura en el liquido: " +  String(tempReadings.getLiquidTemp())+ "°C\n" +
                    "Temperatura superior de histéresis: " + String(tempParams.getTempH()) + "°C\n" +
                    "Temperatura inferior de histéresis: " + String(tempParams.getTempL()) + "°C\n" +
                    "Temperatura superior de cambio de modo: " + String(tempParams.getTempHH()) + "°C\n" +
                    "Temperatura inferior de cambio de modo: " + String(tempParams.getTempLL()) + "°C\n";
      bot.sendMessage(chat_id, statusString, "Markdown");
    }

    if (text == "/getTemp")
    {
      String tempString = "Temperatura en la camara: " + String(tempReadings.getChamberTemp()) + "°C\n" +
                          "Temperatura en el liquido: " + String(tempReadings.getLiquidTemp()) + "°C\n";
      bot.sendMessage(chat_id, tempString, "Markdown");
    }

    if (text == "/getChamberTemp")
    {
      String tempString = "Temperatura en la camara: " + String(tempReadings.getChamberTemp()) + "°C\n";
      bot.sendMessage(chat_id, tempString, "Markdown");
    }

    if (text == "/getLiquidTemp")
    {
      String tempString = "Temperatura en el liquido: " + String(tempReadings.getLiquidTemp()) + "°C\n";
      bot.sendMessage(chat_id, tempString, "Markdown");
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
void vCheckNewMessagesTask(void *px)
{
  while(1)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages, px);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    vTaskDelay(BOT_PERIOD);
  }

  /* Must not exit, but if you leave the while(1) you can delete the task */
  vTaskDelete(NULL);
}

/* sensor readings in a separate task */
void vReadTempTask(void *px)
{
  TaskParameters pParams = *(struct TaskParameters*)px;
  FridgeTemps    tempReadings = pParams.tempReadings;
  while(1)
  {
      tempReadings.updateTemp();
      vTaskDelay(READ_PERIOD);
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
  TaskParameters pParams = *(struct TaskParameters*)px;
  TempParameters tempParams   = pParams.tempParams;
  FridgeTemps    tempReadings = pParams.tempReadings;
  while(1){
    xTimeCur = xTaskGetTickCount();
    if (xTimeCur < xTimeOff) xTimeOff = xTimeCur;
    canRestart = xTimeCur - xTimeOff > COOL_WAIT ? true : false;
    canStopFan = xTimeCur - xTimeOff > FAN_WAIT  ? true : false;
    
    if (selectedMode != MODE_OFF)
    {
      /* mode changes */
      switch (selectedMode)
      {
        case MODE_AUTO :
          switch (currentMode)
          {
            case UNDEFINED :
              if (tempReadings.getRefTemp() > tempParams.getTempHH()) currentMode = COOLING;
              else if (tempReadings.getRefTemp() < tempParams.getTempLL()) currentMode = HEATING;
              break;
            case HEATING :
              if (tempReadings.getRefTemp() > tempParams.getTempHH()) currentMode = COOLING;
              break;
            case COOLING :
              if (tempReadings.getRefTemp() < tempParams.getTempLL()) currentMode = HEATING;
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
          if (coolingState && (tempReadings.getRefTemp() < tempParams.getTempL())) 
          {
            xTimeOff = xTaskGetTickCount();
            coolingState = false;
          } 
          else if (!(coolingState))
          {
            if (canRestart && (tempReadings.getRefTemp() > tempParams.getTempH()))
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
          if (heatingState && (tempReadings.getRefTemp() > tempParams.getTempH()))
          {
            xTimeOff = xTaskGetTickCount();
            heatingState = false;
          } 
          else if (!(heatingState))
          {
            if (tempReadings.getRefTemp() < tempParams.getTempL())
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
    vTaskDelay(CONTROL_PERIOD);
  }
  /* Must not exit, but if you leave the while(1) you can delete the task */
  vTaskDelete(NULL);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  pinMode(FAN_PIN, OUTPUT); 
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(COOL_PIN, OUTPUT);
  delay(10);
  digitalWrite(FAN_PIN, LOW); 
  digitalWrite(HEAT_PIN, LOW);
  digitalWrite(COOL_PIN, LOW);

  TempParameters tempParams;
  FridgeTemps    tempReadings;
  struct TaskParameters parametersForTasks = { tempParams, tempReadings };

  #ifdef PRINT_ADDRESS_DS18B20
    DeviceAddress tempDeviceAddress;

    int numberOfDevices = tempReadings.getSensorCount();
    
    // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    // Loop through each device, print out address
    for(int i=0;i<numberOfDevices; i++) {
      // Search the wire for address
      if(tempReadings.getAddress(tempDeviceAddress, i)) {
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

  xTaskCreate(vReadTempTask,         "readTemp",    0x2000, NULL, 2, NULL);
  vTaskDelay(1000);
  xTaskCreate(vCheckNewMessagesTask, "checkMsg",    0x2000, (void*) &parametersForTasks, 2, NULL);
  xTaskCreate(vTempControl,          "tempControl", 0x2000, (void*) &parametersForTasks, 2, NULL);
}

void loop()
{
  
}
