

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <EEPROM.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

//We can keep 100 bytes per field in the EEPROM
//ESP-01 has a 4096 bytes long EEPROM
#define IFTTT_TOKEN_START_ADDRESS 100
#define IFTTT_EVENT_START_ADDRESS 200


//define your default values here, if there are different values in config.json, they are overwritten.
const char* ifttt_event;
const char* ifttt_token;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

bool saveDataInEEPROM(int fieldstartaddress, const char* stringdata) {
  // The event name is a string whose length varies
  //The eeprom location at the start address holds the length of the data

  int data_length = strlen(stringdata);

  //Serial.print("Data Length: ");
  //Serial.println(data_length);

  EEPROM.write(fieldstartaddress, data_length);

  EEPROM.commit();




  int dataStartAddress = fieldstartaddress + 1;
  int dataEndAddress = dataStartAddress + data_length;

  for (int addr = 0; addr < data_length; addr++)
  {

    EEPROM.write(addr + dataStartAddress, stringdata[addr]);
    EEPROM.commit();
  }



  return true;
}



const char* getEEPROMData(int fieldstartaddress) {
  // The event name is a string whose length varies
  //The eeprom location at the start address holds the length of the data

  int data_length = EEPROM.read(fieldstartaddress);
  //Serial.println(data_length);
  int dataStartAddress = fieldstartaddress + 1;
  int dataEndAddress = dataStartAddress + data_length;

  char *dataString = (char*)malloc ( data_length * sizeof (char));

  for (int addr = 0; addr < data_length; addr++)
  {
    dataString[addr] = EEPROM.read(addr + dataStartAddress);
    /*Serial.print("Char at ");
      Serial.print(addr + dataStartAddress);
      Serial.print(" is: ");
      Serial.println(dataString[addr]);*/
  }

  //Serial.println(dataString);
  return dataString;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(512);


  // Lets look at the data that exists in the EEPROM
  ifttt_event = getEEPROMData(IFTTT_EVENT_START_ADDRESS);
  ifttt_token = getEEPROMData(IFTTT_TOKEN_START_ADDRESS);


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter IFTTTEventName("ifttt_event", "ifttt_event", ifttt_event, 40);

  WiFiManagerParameter IFTTTToken("ifttt_token", "ifttt_token", ifttt_token, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&IFTTTEventName);
  wifiManager.addParameter(&IFTTTToken);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("TriggerAlarm", "TriggerAlarm")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep

    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("Connected to AP");

  if (shouldSaveConfig)
  {
    //read updated parameters
    ifttt_event = IFTTTEventName.getValue();
    ifttt_token = IFTTTToken.getValue();

    //We save the Data we recieved in EEPROM.
    saveDataInEEPROM(IFTTT_TOKEN_START_ADDRESS, ifttt_token);
    saveDataInEEPROM(IFTTT_EVENT_START_ADDRESS, ifttt_event);
  }

  /*At this Point, we have connected to the User supplied Access Point
    and we have access to the user
  */
  if (!shouldSaveConfig)
  {
    Serial.println(ifttt_event);
    Serial.println(ifttt_token);


   

    HTTPClient http;

    char* URL_P1 = "http://maker.ifttt.com/trigger/";
    char * URL_P2 = "/with/key/";

    
    int URLLength = strlen(URL_P1) + strlen(ifttt_event) + strlen(URL_P2) + strlen(ifttt_token) - 1;

    char *URL = (char*)calloc ( URLLength, sizeof (char));

    strcat(URL, URL_P1);
    strcat(URL, ifttt_event);
    strcat(URL, URL_P2);
    strcat(URL, ifttt_token);

    Serial.println(URL);
    
    http.begin(URL);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      Serial.println('OK');
    }



    ESP.deepSleep(0);

  }






}



void loop() {
  // put your main code here, to run repeatedly:


}
