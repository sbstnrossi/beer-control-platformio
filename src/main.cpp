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
float refTemp, tempH, tempHH, tempL, tempLL;
UBaseType_t selectedMode = MODE_OFF;
UBaseType_t currentMode  = UNDEFINED;

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

    if (text == "/ledon")
    {
      digitalWrite(ledPin, LOW); // turn the LED on (HIGH is the voltage level)
      ledStatus = 1;
      bot.sendMessage(chat_id, "Led is ON", "");
    }

    if (text == "/ledoff")
    {
      ledStatus = 0;
      digitalWrite(ledPin, HIGH); // turn the LED off (LOW is the voltage level)
      bot.sendMessage(chat_id, "Led is OFF", "");
    }

    if (text == "/status")
    {
      if (ledStatus)
      {
        bot.sendMessage(chat_id, "Led is ON", "");
      }
      else
      {
        bot.sendMessage(chat_id, "Led is OFF", "");
      }
    }

    if (text == "/getTemp")
    {
      /* TODO add semaphore */
      String tempString = "Temperatura en DS18B20: " + readDSTempStringC(0) + "°C\n";
      bot.sendMessage(chat_id, tempString, "Markdown");
    }

    if (text == "/start")
    {
      String welcome = "Welcome to Universal Arduino Telegram Bot library, " + from_name + ".\n";
      welcome += "This is Flash Led Bot example.\n\n";
      welcome += "/ledon : to switch the Led ON\n";
      welcome += "/ledoff : to switch the Led OFF\n";
      welcome += "/getTemp : Returns current DS18B20 temperature\n";
      welcome += "/status : Returns current status of LED\n";
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
void tempControl(void* px)
{
  bool coolingState = false;
  bool heatingState = false;
  bool blowingState = false;
  bool canRestart   = false;
  bool canStopFan   = false;
  TickType_t xTimeOff = xTaskGetTickCount();
  TickType_t xTimeCur;
  while(1){
    xTimeCur = xTaskGetTickCount();
    if (xTimeCur < xTimeOff) xTimeOff = xTimeCur;
    canRestart = xTimeCur - xTimeOff > COOL_WAIT ? true : false;
    canStopFan = xTimeCur - xTimeOff > FAN_WAIT  ? true : false;
    if (!(MODE_OFF))
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
          if (digitalRead(COOL_PIN)) digitalWrite(COOL_PIN, LOW);
          if (digitalRead(HEAT_PIN)) digitalWrite(HEAT_PIN, LOW);
          coolingState = false;
          heatingState = false;
          if (blowingState && canStopFan)
          {
            digitalWrite(FAN_PIN,  LOW);
            blowingState = false;
          }
          break;
        case COOLING :
          if (digitalRead(HEAT_PIN)) digitalWrite(HEAT_PIN, LOW);
          if (coolingState && (refTemp < tempL)) 
          {
            xTimeOff = xTaskGetTickCount();
            digitalWrite(COOL_PIN, LOW);
            coolingState = false;
          } 
          else if (!(coolingState))
          {
            if (canRestart && (refTemp > tempH))
            {
              digitalWrite(COOL_PIN, HIGH);
              digitalWrite(FAN_PIN,  HIGH);
              coolingState = true;
              blowingState = true;
            }
            else if (blowingState && canStopFan)
            {
              digitalWrite(FAN_PIN,  LOW);
              blowingState = false;
            }
          }
          break;
        case HEATING :
          if (digitalRead(COOL_PIN)) digitalWrite(COOL_PIN, LOW);
          if (heatingState && (refTemp > tempH)) 
          {
            xTimeOff = xTaskGetTickCount();
            digitalWrite(HEAT_PIN, LOW);
            heatingState = false;
          } 
          else if (!(heatingState))
          {
            if (refTemp < tempL)
            {
              digitalWrite(HEAT_PIN, HIGH);
              digitalWrite(FAN_PIN,  HIGH);
              heatingState = true;
              blowingState = true;
            }
            else if (blowingState && canStopFan)
            {
              digitalWrite(FAN_PIN,  LOW);
              blowingState = false;
            }
          }
          break;
        default:
          currentMode = UNDEFINED;
      }
    } else {
      currentMode = UNDEFINED;
      digitalWrite(COOL_PIN, LOW);
      digitalWrite(HEAT_PIN, LOW);
      digitalWrite(FAN_PIN,  LOW);
      coolingState = false;
      heatingState = false;
      blowingState = false;
    }
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

  xTaskCreate(vCheckNewMessagesTask, "checkMsg", 0x2000, NULL, 2, NULL);
}

void loop()
{
  
}
