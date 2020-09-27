/*
Configuration
*/
const char* versionStr = "20200927v0.4";
#define LoggingWithTimeout

#ifdef LoggingWithTimeout
#define logTimeout (1800) // 60 min * 60sec = 1 std
#endif 

#define OTA_active
//#define WifiManager_active
//#define MyESP01
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();   //https://github.com/sui77/rc-switch


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

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2;  // D4 on NodeMCU Controls the onboard LED.
const int GPIO_D2 = 4;  // D2 on NodeMCU
const int GPIO_D5 = 14; // D5 on NodeMCU
const int GPIO_D6 = 12; // D6 on NodeMCU
const int GPIO_D7 = 13; // D7 on NodeMCU

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
unsigned long lastMsg = 0;
unsigned long lastTry = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
const char* mqtt_server = "192.168.2.127";
static bool MQTTConnection = false;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
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
    #else 
    ESP.restart();
    #endif
  } else {
    #ifndef MyESP01
    digitalWrite(PIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    #else
    ESP.restart();
    #endif
  }

}

void reconnect() {
  // Loop until we're reconnected
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("RFC/outTopic", versionStr);
      // ... and resubscribe
      client.subscribe("RFC/inTopic");
      MQTTConnection = true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
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
  pinMode(GPIO_D2, OUTPUT);
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

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  mySwitch.enableTransmit(GPIO_D6);
  // OTA (only after connection is established)
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("myesp8266_RFC");

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
       
      if(cnt%2==0)
      {
        Serial.println("On");
        mySwitch.switchOn("00000", "10100");
        digitalWrite(GPIO_D5, HIGH); 
      }else{
        Serial.println("Off");
        mySwitch.switchOff("10100", "10100");
        digitalWrite(GPIO_D5, LOW);
      }
    }
  }
}
