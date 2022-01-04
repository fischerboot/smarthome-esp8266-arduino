/*
Configuration
*/
const char* versionStr = "20210314v0.8";
#define LoggingWithTimeout

#ifdef LoggingWithTimeout
#define logTimeout (1800) // 60 min * 60sec = 1 std
#endif 

#define OTA_active
//#define WifiManager_active
//#define MyESP01
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <RCSwitch.h>

//RCSwitch mySwitch = RCSwitch();   //https://github.com/sui77/rc-switch
#define TRANSMIT_RETRY (5)
//needed for Telnet?
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
enum LogLevel{
  Verbose   = 0,
  Debug     = 1,
  Error     = 2,
  Warning   = 3,
  Info      = 4
};
static LogLevel UsedLevel = Verbose; 
/*
void logger(char logInput,uint8_t level);
void logger(unsigned int logInput,uint8_t level);
void logger(const char* logInput,uint8_t level);
void logger(char* logInput,uint8_t level);
void logger(byte logInput,uint8_t level);
void logger(int* logInput,uint8_t level);*/
//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#ifdef WifiManager_active
#include <WiFiManager.h>          //https://github.com/kentaylor/WiFiManager
#else 
#include <WlanConfig.h>
#endif
#include <ArduinoOTA.h>

// MQTT Client
#include <PubSubClient.h>
const char* charMqttTopic_LED_Cmnd = "home/light/test/cmnd";
const char* charMqttTopic_LED_State = "home/light/test/state";

static char jsonData[80];
#define MAX_TOKEN_STRING 10
static char cTokenCode [MAX_TOKEN_STRING];
static char cTokenDevice [MAX_TOKEN_STRING];
static char cTokenState [MAX_TOKEN_STRING];
static char cTokenCount [MAX_TOKEN_STRING];
static char cTokenDelay [MAX_TOKEN_STRING];

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2;  // D4 on NodeMCU Controls the onboard LED.
const int GPIO_0 = 2; // ESP-01 Pin
const int GPIO_D0 = 16;  // D2 on NodeMCU
const int GPIO_D2 = 4;  // D2 on NodeMCU
const int GPIO_D5 = 14; // D5 on NodeMCU
const int GPIO_D6 = 12; // D6 on NodeMCU
const int GPIO_D7 = 13; // D7 on NodeMCU
const int GPIO_D8 = 15; // D8 on NodeMCU

//ultrasonic example
int maximumRange = 300;
int minimumRange = 2;
long Abstand;
long Dauer;
void ultrasonicLoop();

//from MK Smarthome
int greenLedVal = 0;
int redLedVal = 0;
int blueLedVal = 0;

enum Rainbow {REDD, ORANGE, YELOW, GREN, BLU, INDIGO, VIOLET};

Rainbow currentRainbow = REDD;

#define REDPIN GPIO_D7
#define GREENPIN GPIO_D8
#define BLUEPIN GPIO_D6
// adjust the PWM range
// see https://esp8266.github.io/Arduino/versions/2.0.0/doc/reference.html#analog-output

#ifndef WifiManager_active
#ifndef WlanConfig_h
#define STASSID "------------"
#define STAPSK  "------------"
#endif
const char* ssid = STASSID;
const char* password = STAPSK;
#endif
// MQTT Defines 
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long cnt = 0; // 2 seconds
unsigned int help = 0; // 2 seconds
unsigned long lastMsg = 0;
unsigned long lastTry = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
const char* mqtt_server = "192.168.2.127";
static bool MQTTConnection = false;

#define MAX_TELNET_CLIENTS 2

uint8_t i;
bool ConnectionEstablished; // Flag for successfully handled connection
WiFiServer TelnetServer(23);
WiFiClient TelnetClient[MAX_TELNET_CLIENTS];

int bewegungsstatus=0;
// from https://hoeser-medien.de/2017/07/openhab-433-mhz-sender-mit-json-format-ueber-mqtt-anbinden/#codesyntax_4
// ---------------------------------------------------------------------------------------- json_lookup_char
bool json_lookup_char ( const char* cSource, const char* cToken, char cReply[]){
  char *ptr;
  cReply[0]='\0'; // set null to 1st char in target
  logger("\njson_lookup_char:",Verbose);
  logger(cSource,Verbose);
  logger("Token >",Verbose);
  logger(cToken[0],Verbose);
  //logger("",Verbose);
  ptr = strstr (cSource, cToken); // set pointer to 1st char in cSource matching token
  if(!ptr){
   logger("json_lookup_char > ",Error);
   logger(cToken[0],Error);
   logger("< not found\n\r",Error);
  return false;
  }
  logger("< ok",Verbose);
  logger("1 *ptr >",Verbose);
  logger(*ptr,Verbose);
  logger("<",Verbose);
  logger("Token >",Verbose);
  logger(":\n\r",Verbose);
  ptr = strstr (ptr, ":"); // set pointer to :
  if(!ptr){
   logger(" json_lookup_char > ",Error);
   logger(": not found\n\r",Error);
  return false;
  }
  logger("< ok",Verbose);
  ptr++;
  //if (*ptr=='"')
  // ptr ++;
  // read char and copy to
  for (int i=0, x=0;i<MAX_TOKEN_STRING+1;i++){
   if (!ptr)
   break;
   if (*ptr==','|| *ptr=='}') // read until ,}
    break;
    if (*ptr!=' '&& *ptr!='"') { // skip space + "
      cReply[x ] = *ptr; // copy single char to target
      cReply[x+1] = '\0'; // set end of string
      x++;
    }
    ptr++;
  } // for
  logger(" result >",Verbose);
  logger(cReply,Verbose);
  logger("<",Verbose);
  return true;
}
// from https://hoeser-medien.de/2017/07/openhab-433-mhz-sender-mit-json-format-ueber-mqtt-anbinden/#codesyntax_4
void callback2(char* topic, byte* payload, unsigned int length) {
  int iTokenState =0;
  int iTokenCount =0;
  int iTokenDelay =0;
  #ifdef RESETWATCHDOG
  wdt_reset();
  #endif
  logger("Message arrived > [",Verbose);
  logger(topic,Verbose);
  logger("], length: ",Verbose);
  logger(length,Verbose);
  logger("]",Verbose);
  for (unsigned int i=0;i<length;i++) {
    jsonData[i] = (char)payload[i];
    jsonData[i+1] = '\0';
    logger(payload[i],Verbose);
  }
  logger("]\n\r",Verbose);
  if(strcmp(topic,charMqttTopic_LED_Cmnd)==0) { // whatever you want for this topic
    logger("MQTT > 433 cmd received :\n\r",Debug);
    // 10011 10000 für Steckdose A
    // JSON {code:"10011", device:"10000", state:1, count:1, delay:200}
    // MQTT Message arrived > [house/433/ardu1/cmnd], length: 71] {code:00001 , device:10000, state:1, count:3. delay:100 }
    if( json_lookup_char(jsonData,"code" , cTokenCode) ==false) return;
    if( json_lookup_char(jsonData,"device", cTokenDevice) ==false) return;
    if( json_lookup_char(jsonData,"state" , cTokenState) ==false) return;
    iTokenState = atoi(cTokenState);
    // Serial.print ("iTokenState > ");Serial.print (iTokenState);Serial.println(" <");
    json_lookup_char(jsonData,"count" , cTokenCount);
    iTokenCount = atoi(cTokenCount);
    // Serial.print ("iTokenCount > ");Serial.print (iTokenCount);Serial.println(" <");
    if(iTokenCount==0)
    iTokenCount = 1;
    json_lookup_char(jsonData,"delay" , cTokenDelay);
    iTokenDelay = atoi(cTokenDelay);
    // Serial.print ("iTokenDelay > ");Serial.print (iTokenDelay);Serial.println(" <");
    if(iTokenDelay==0)
    iTokenDelay = 50;
    logger("433-send> code>",Debug); logger(cTokenCode,Debug);
    logger("< device>",Debug); logger(cTokenDevice,Debug);
    logger("< state>",Debug); logger(iTokenState,Debug);
    logger("< count>",Debug); logger(iTokenCount,Debug);
    logger("< delay>",Debug); logger(iTokenDelay,Debug);
    logger(" > ",Debug);
    // 433-send> code: device: state:1 count:3 delay:100 >
    #ifdef RESETWATCHDOG
    wdt_reset();
    #endif
    switch (iTokenState)
    {
    case 0:
      logger(" mySwitch.switchOff >",Debug);
      for (int i=0;i<iTokenCount;i++) {
        logger("+",Debug);
        delay(iTokenDelay);
      }
     client.publish(charMqttTopic_LED_State,"0");
      break;
    case 1:
      logger(" mySwitch.switchOn >",Debug);
      for (int i=0;i<iTokenCount;i++) {
        //mySwitch.switchOn("00001", "10000");
        logger("+",Debug);
        delay(iTokenDelay);
      }
      client.publish(charMqttTopic_LED_State,"1");
      break;
    default:
      logger(" Error >",Error);
      client.publish(charMqttTopic_LED_State,"invalid state");
      break;
    }
    logger(" DONE\n\r",Info);
  } else {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
      #ifndef MyESP01
      digitalWrite(PIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
      // it is active low on the ESP-01)
      analogWrite(REDPIN, 0);
      analogWrite(GREENPIN, 0);
      analogWrite(BLUEPIN, 0);
      #else 
      mySwitch.switchOff("10100", "10000");
      logger("RFC Off\n\r",Debug);
      //ESP.restart();
      #endif
    } else {
      #ifndef MyESP01
      digitalWrite(PIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
      analogWrite(REDPIN, 255);
      analogWrite(GREENPIN, 255);
      analogWrite(BLUEPIN, 255);
      #else
      mySwitch.switchOn("10100", "10000");
      logger("RFC On\n\r",Debug);
      //ESP.restart();
      #endif
    }
  }// 433 cmd received
  
} // end of function

void callback(char* topic, byte* payload, unsigned int length) {
  logger("Message arrived [",Verbose);
  logger(topic,Verbose);
  logger("] ",Verbose);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    #ifndef MyESP01
    digitalWrite(PIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
    analogWrite(REDPIN, 255);
    analogWrite(GREENPIN, 255);
    analogWrite(BLUEPIN, 255);
    #else 
    mySwitch.switchOff("10100", "10000");
    logger("RFC Off\n\r",Debug);
    //ESP.restart();
    #endif
  } else {
    #ifndef MyESP01
    digitalWrite(PIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    analogWrite(REDPIN, 0);
    analogWrite(GREENPIN, 0);
    analogWrite(BLUEPIN, 0);
    #else
    mySwitch.switchOn("10100", "10000");
    logger("RFC On\n\r",Debug);
    //ESP.restart();
    #endif
  }

}

void reconnect() {
  // Loop until we're reconnected
    logger("Attempting MQTT connection...",Info);
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      logger("connected\n",Info);
      // Once connected, publish an announcement...
      client.publish("LED/outTopic", versionStr);
      // ... and resubscribe
      client.subscribe("LED/inTopic");
      client.subscribe("test/switch");
      client.subscribe(charMqttTopic_LED_Cmnd); // house/433/ardu1/cmnd
      logger("MQTT sub >",Info);
      logger(charMqttTopic_LED_Cmnd,Info);
      MQTTConnection = true;
    } else {
      logger("failed, rc=",Warning);
      logger(client.state(),Warning);
      logger(" try again in 5 seconds\n",Warning);
      // Wait 5 seconds before retrying
      MQTTConnection = false;
    }
}

void setup() {
  // put your setup code here, to run once:
  // initialize the LED digital pin as an output.
  #ifndef MyESP01
  pinMode(PIN_LED, OUTPUT);
  pinMode(GPIO_D5, OUTPUT);
  pinMode(GPIO_D0, INPUT);
  analogWriteRange(1023);
  resetOutputs();
  #endif
  Serial.begin(115200);
  Serial.print("\n Starting Version:");
  Serial.println(versionStr);
  #ifdef WifiManager_active
  unsigned long startedAt = millis();
  //WiFi.printDiag(Serial); //Remove this line if you do not want to see WiFi password printed
  Serial.println("Opening configuration portal");
  #ifndef MyESP01
  digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
  #endif
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;  
  // Dont want Debug for the moment.
  wifiManager.setDebugOutput(true);
  
  //sets timeout in seconds until configuration portal gets turned off.
  //If not specified device will remain in configuration mode until
  //switched off via webserver.
  if (WiFi.SSID()!="") wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.
  
  //it starts an access point 
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.startConfigPortal("ESP8266","password")) {//Delete these two parameters if you do not want a WiFi password on your configuration access point
     Serial.println("Not connected to WiFi but continuing anyway.");
  } else {
     //if you get here you have connected to the WiFi
     Serial.println("connected...yeey :)");
     }
   #ifndef MyESP01
  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.
      // For some unknown reason webserver can only be started once per boot up 
      // so webserver can not be used again in the sketch.
   #endif 
  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis()- startedAt);
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("failed to connect, finishing setup anyway");
  } else{
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());
  }
  #else
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  #endif

  Serial.println("Starting Telnet server");
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // OTA (only after connection is established)
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("myesp8266_LED");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  Telnet();  // Handle telnet connections
  
  
  #ifndef MyESP01
  /*delay(1000);
  digitalWrite(GPIO_D5, HIGH);
  delay(1000);
  digitalWrite(GPIO_D5, LOW);*/
  #endif
  unsigned long now = millis(); 
  if (!client.connected()) {
    if (now - lastTry > 5000) {
      lastTry = now;
    reconnect();
    }
  }
  client.loop();
  if(MQTTConnection){
    if (now - lastMsg > 2000) {
      lastMsg = now;
      cnt++;
      snprintf (msg, MSG_BUFFER_SIZE, "%lu Sec alive", cnt*2);
      client.publish("RFC/alive", msg);
      /*bewegungsstatus=digitalRead(GPIO_D2); //ier wird der Pin7 ausgelesen. Das Ergebnis wird unter der Variablen „bewegungsstatus“ mit dem Wert „HIGH“ für 5Volt oder „LOW“ für 0Volt gespeichert.
      if (bewegungsstatus == HIGH){
        digitalWrite(PIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
        // but actually the LED is on; this is because
        // it is active low on the ESP-01)
        mySwitch.switchOn("10100", "10000");
        }else {
          digitalWrite(PIN_LED, HIGH);   // Turn the LED on (Note that LOW is the voltage level
          // but actually the LED is on; this is because
          // it is active low on the ESP-01)
           mySwitch.switchOff("10100", "10000");
         } //Programmabschnitt des else-Befehls schließen.
         */

      if(cnt%2==0)
      {
        logger(".",Debug);
        //mySwitch.switchOn("10101", "10000");
        //mySwitch.switchOn("10100", "10000");
        //digitalWrite(GPIO_D5, HIGH); 
      }else{
        logger(" ",Debug);
        //mySwitch.switchOff("10101", "10000");
        //mySwitch.switchOff("10100", "10000");
        //digitalWrite(GPIO_D5, LOW);
      }
  }
    if(now%500==0){
      /*
       switch(currentRainbow)
        {
          case REDD:
            greenLedVal = 0;
            redLedVal = 1023;
            blueLedVal = 0;
            currentRainbow = ORANGE;
            break;
          case ORANGE:
            greenLedVal = 700;
            redLedVal = 1023;
            blueLedVal = 0;
            currentRainbow = YELOW;
            break;
          case YELOW:
            greenLedVal = 1023;
            redLedVal = 1023;
            blueLedVal = 0;
            currentRainbow = GREN;
            break; 
          case GREN:
            greenLedVal = 1023;
            redLedVal = 0;
            blueLedVal = 0;
            currentRainbow = BLU;
            break; 
          case BLU:
            greenLedVal = 0;
            redLedVal = 0;
            blueLedVal = 1023;
            currentRainbow = INDIGO;
            break; 
          case INDIGO:
            greenLedVal = 0;
            redLedVal = 1023;
            blueLedVal = 1023;
            currentRainbow = VIOLET;
            break; 
          case VIOLET:
            greenLedVal = 100;
            redLedVal = 1023;
            blueLedVal = 588;
            currentRainbow = REDD;
            break;
        }
        analogWrite(REDPIN, redLedVal);
        analogWrite(GREENPIN, greenLedVal);
        analogWrite(BLUEPIN, blueLedVal);
      }*/
      ultrasonicLoop();
    }
  }
}

void TelnetMsg(char* text){
  for(i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] || TelnetClient[i].connected())
    {
      TelnetClient[i].print(text);
    }
  }
  delay(10);  // to avoid strange characters left in buffer
}
void TelnetMsg(char text){
  for(i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] || TelnetClient[i].connected())
    {
      TelnetClient[i].print(text);
    }
  }
  delay(10);  // to avoid strange characters left in buffer
}
      
void Telnet(){
  // Cleanup disconnected session
  for(i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] && !TelnetClient[i].connected())
    {
      Serial.print("Client disconnected ... terminate session "); Serial.println(i+1); 
      TelnetClient[i].stop();
    }
  }
  
  // Check new client connections
  if (TelnetServer.hasClient())
  {
    ConnectionEstablished = false; // Set to false
    
    for(i = 0; i < MAX_TELNET_CLIENTS; i++)
    {
      // Serial.print("Checking telnet session "); Serial.println(i+1);
      
      // find free socket
      if (!TelnetClient[i])
      {
        TelnetClient[i] = TelnetServer.available(); 
        
        Serial.print("New Telnet client connected to session "); Serial.println(i+1);
        
        TelnetClient[i].flush();  // clear input buffer, else you get strange characters
        TelnetClient[i].println("Welcome!");
        
        TelnetClient[i].print("Millis since start: ");
        TelnetClient[i].println(millis());
        
        TelnetClient[i].print("Free Heap RAM: ");
        TelnetClient[i].println(ESP.getFreeHeap());
        
        TelnetClient[i].print("Version: ");
        TelnetClient[i].println(versionStr);
        
        TelnetClient[i].println("----------------------------------------------------------------");
        
        ConnectionEstablished = true; 
        
        break;
      }
      else
      {
        // Serial.println("Session is in use");
      }
    }

    if (ConnectionEstablished == false)
    {
      Serial.println("No free sessions ... drop connection");
      TelnetServer.available().stop();
      // TelnetMsg("An other user cannot connect ... MAX_TELNET_CLIENTS limit is reached!");
    }
  }

  for(i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] && TelnetClient[i].connected())
    {
      if(TelnetClient[i].available())
      { 
        //get data from the telnet client
        while(TelnetClient[i].available())
        {
          Serial.write(TelnetClient[i].read());
        }
      }
    }
  }
}


void resetOutputs() {
  analogWrite(REDPIN, 1023);
  analogWrite(GREENPIN, 1023);
  analogWrite(BLUEPIN, 1023);
  
  analogWrite(REDPIN, 0);
  analogWrite(GREENPIN, 0);
  analogWrite(BLUEPIN, 0);
}

void ultrasonicLoop(){
    // Abstandsmessung wird mittels des 10us langen Triggersignals gestartet
  digitalWrite(GPIO_D5, HIGH);
  delayMicroseconds(10);
  digitalWrite(GPIO_D5, LOW);
  // Nun wird am Echo-Eingang gewartet, bis das Signal aktiviert wurde
  // und danach die Zeit gemessen, wie lang es aktiviert bleibt
  Dauer = pulseIn(GPIO_D0, HIGH);
  // Nun wird der Abstand mittels der aufgenommenen Zeit berechnet
  Abstand = Dauer/58.2;
  // Überprüfung ob gemessener Wert innerhalb der zulässingen Entfernung liegt
  if (Abstand >= maximumRange || Abstand <= minimumRange) {
    // Falls nicht wird eine Fehlermeldung ausgegeben.
    logger("Abstand ausserhalb des Messbereichs",Debug);
    logger("-----------------------------------\n\r",Debug);
    analogWrite(REDPIN, 1023);
    analogWrite(GREENPIN, 1023);
    analogWrite(BLUEPIN, 1023);
  } else {
    // Der berechnete Abstand wird in der seriellen Ausgabe ausgegeben
    logger("Der Abstand betraegt:",Debug);
    char cstr[16];
    ltoa(Debug, cstr, 10);
    logger(cstr,Debug);
    logger("cm\n\r",Debug);
    logger("-----------------------------------\n\r",Debug);
    analogWrite(REDPIN, 0);
    analogWrite(GREENPIN, 0);
    analogWrite(BLUEPIN, 0);
  }
}

/*
void logger(String logInput,uint8_t level){
  //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg((char*)logInput);
  }
}*/
void logger(char logInput,uint8_t level){
  //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg(logInput);
  }
}
void logger(char* logInput,uint8_t level){
  //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg(logInput);
  }
}
void logger(const char* logInput,uint8_t level){
  //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg((char*)logInput);
  }
}
/*
void logger(unsigned int logInput,uint8_t level){
  //uint8_t logLevel = level;
  if(UsedLevel <= level){
    char cstr[16];
    itoa(logInput, cstr, 10);
    Serial.print(cstr);
    TelnetMsg(cstr);
  }
}

void logger(byte logInput,uint8_t level){
    //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg((char*)logInput);
  }
}
void logger(int* logInput,uint8_t level){
    //uint8_t logLevel = level;
  if(UsedLevel <= level){
    Serial.print(logInput);
    TelnetMsg((char*)logInput);
  }
}*/
