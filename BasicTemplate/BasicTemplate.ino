/*
This example will open a configuration portal for 60 seconds when first powered up.
ConfigOnSwitch is a a bettter example for most situations but this has the advantage 
that no pins or buttons are required on the ESP8266 device at the cost of delaying 
the user sketch for the period that the configuration portal is open.

Also in this example a password is required to connect to the configuration portal 
network. This is inconvenient but means that only those who know the password or those 
already connected to the target WiFi network can access the configuration portal and 
the WiFi network credentials will be sent from the browser over an encrypted connection and
can not be read by observers.
*/
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/kentaylor/WiFiManager
#include <ArduinoOTA.h>
#include <SevSeg.h>               // https://github.com/DeanIsMe/SevSeg.git

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.

SevSeg sevseg; //Instantiate a seven segment controller object

void setup() {
  // put your setup code here, to run once:
  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(115200);
  Serial.println("\n Starting");
  unsigned long startedAt = millis();
  WiFi.printDiag(Serial); //Remove this line if you do not want to see WiFi password printed
  Serial.println("Opening configuration portal");
  digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;  
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
  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.
      // For some unknown reason webserver can only be started once per boot up 
      // so webserver can not be used again in the sketch.
    
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

  // OTA (only after connection is established)
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

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

  pinMode(12, OUTPUT); //latch
   
  //SevenSeg Settings 
  byte numDigits = 4;
  byte digitPins[] = {16, 4, 5, 14}; // TBD D0, D2, D1,D5
  byte segmentPins[] = {1, 2, 3, 4, 5, 6, 7, 8}; // TBD
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected
  
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments,
  updateWithDelays, leadingZeros, disableDecPoint);
}


void loop() {
  ArduinoOTA.handle();

  static unsigned long timer = millis();
  static int deciSeconds = 0;
  
  if (millis() - timer >= 100) {
    timer += 100;
    deciSeconds++; // 100 milliSeconds is equal to 1 deciSecond
    
    if (deciSeconds == 10000) { // Reset to 0 after counting for 1000 seconds.
      deciSeconds=0;
    }
    sevseg.setNumber(deciSeconds, 1);
    digitalWrite(2,deciSeconds%2);
  }

  sevseg.refreshDisplay(); // Must run repeatedly
 
  
}
