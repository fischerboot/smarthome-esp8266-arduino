/*
Configuration
*/
#define OTA_active
//#define WifiManager_active
#define MyESP01
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#ifdef WifiManager_active
#include <WiFiManager.h>          //https://github.com/kentaylor/WiFiManager
#else 
#include <WlanConfig.h>
#endif
#include <ArduinoOTA.h>

//BMP Sensor Libs
#include <Wire.h>
//#include <Adafruit_BMP280.h>     //https://github.com/adafruit/Adafruit_BMP280_Library.git
#include <Adafruit_BME280.h>       //https://github.com/adafruit/Adafruit_BME280_Library.git
// MQTT Client
#include <PubSubClient.h>

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2;  // D4 on NodeMCU Controls the onboard LED.
const int GPIO_D2 = 4;  // D2 on NodeMCU
const int GPIO_D5 = 14; // D5 on NodeMCU
const int GPIO_D6 = 12; // D6 on NodeMCU
const int GPIO_D7 = 13; // D7 on NodeMCU

// Senor Defines
Adafruit_BME280 bme; // I2C
#define SEALEVELPRESSURE_HPA (1013.25)
static bool SensorWiringError = false;
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
unsigned long lastMsg = 0;
unsigned long lastTry = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
float valueTemp = 0;
float valueTemp_Pre = 0;
float valueHum = 0;
float valueHum_Pre = 0;
float valuePres = 0;
float valuePres_Pre = 0;
float valueAlt = 0;
float valueAlt_Pre = 0;
float epsilon = 0.1;
const char* mqtt_server = "192.168.2.104";
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
    digitalWrite(GPIO_D5, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
    #else 
    ESP.restart();
    #endif
  } else {
    #ifndef MyESP01
    digitalWrite(GPIO_D5, HIGH);  // Turn the LED off by making the voltage HIGH
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
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
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
  #endif
  Serial.begin(115200);
  Serial.println("\n Starting");
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
  if (!bme.begin(BME280_ADDRESS_ALTERNATE)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    SensorWiringError = true;
  }

 // /* Default settings from datasheet. */
 // bme.setSampling(Adafruit_BME280::MODE_NORMAL,     /* Operating Mode. */
 //                 Adafruit_BME280::SAMPLING_X2,     /* Temp. oversampling */
 //                 Adafruit_BME280::SAMPLING_X16,    /* Pressure oversampling */
 //                 Adafruit_BME280::FILTER_X16,      /* Filtering. */
 //                 Adafruit_BME280::STANDBY_MS_500); /* Standby time. */

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // OTA (only after connection is established)
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("myesp8266");

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
  /*delay(1000);
  digitalWrite(GPIO_D5, HIGH);
  delay(1000);
  digitalWrite(GPIO_D5, LOW);*/
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
      if(!SensorWiringError){
        valueTemp =bme.readTemperature();
      }
      if (fabs(valueTemp-valueTemp_Pre) < epsilon){
        //Just Log
        Serial.print("Temperature = ");
        Serial.print(valueTemp);
        Serial.println(" *C");
      }else{ 
        // Publish new value
        snprintf (msg, MSG_BUFFER_SIZE, "%2.2f °C", valueTemp);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("BME/Temp", msg);
        valueTemp_Pre = valueTemp;
      }
      if(!SensorWiringError){
        valuePres =(bme.readPressure()/ 100.0F);
      }
      if (fabs(valuePres-valuePres_Pre) < epsilon){
        //Just Log
        Serial.print("Pressure = ");
        Serial.print(valuePres);
        Serial.println(" hPa");
      }else{ 
        // Publish new value
        snprintf (msg, MSG_BUFFER_SIZE, "%.1f hPa", valuePres);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("BME/Pres", msg);
        valuePres_Pre = valuePres;
      }
      if(!SensorWiringError){
        valueAlt =bme.readAltitude(SEALEVELPRESSURE_HPA);
      }
      if (fabs(valueAlt-valueAlt_Pre) < epsilon){
        //Just Log
        Serial.print("Approx. Altitude = ");
        Serial.print(valueAlt);
        Serial.println(" m");
      }else{ 
        // Publish new value
        snprintf (msg, MSG_BUFFER_SIZE, "%.2f m", valueAlt);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("BME/Alt", msg);  
        valueAlt_Pre = valueAlt;
      }
      if(!SensorWiringError){
        valueHum =bme.readHumidity();
      }
      if (fabs(valueHum-valueHum_Pre) < epsilon){
        //Just Log
        Serial.print("Humidity = ");
        Serial.print(valueHum);
        Serial.println(" %");
      }else{ 
        // Publish new value
        snprintf (msg, MSG_BUFFER_SIZE, "%.1f %%", valueHum);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("BME/Hum", msg); 
        valueHum_Pre = valueHum;
      } 
    }
  }
}
