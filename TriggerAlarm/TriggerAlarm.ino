

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <EEPROM.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

extern "C" {
#include <user_interface.h>
}

//We can keep 100 bytes per field in the EEPROM
//ESP-01 has a 4096 bytes long EEPROM
#define IFTTT_TOKEN_START_ADDRESS 100
#define IFTTT_EVENT_START_ADDRESS 200

#define ALLOWED_CHARACTER_LOWER_VALUE 48
#define ALLOWED_CHARACTER_UPPER_VALUE 122

#define AP_SSID "TriggerAlarm"
#define AP_PASSWORD "TriggerAlarm"

#define MAC_LENGTH 6


//define your default values here, if there are different values in config.json, they are overwritten.
String s_ifttt_event;
String s_ifttt_token;

const char* c_ifttt_event;
const char* c_ifttt_token;


//flag for saving data
bool shouldSaveConfig = false;
bool sensorTrigger = false;


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
  int totalValidCharacters = 0;
  for (int addr = 0; addr < data_length; addr++)
  {
    if (allowedCharacter(stringdata[addr]))
    {
      EEPROM.write(addr + dataStartAddress, stringdata[addr]);
      EEPROM.commit();
      totalValidCharacters = totalValidCharacters + 1;
    }
  }
  //Update the length of the valid characters
  EEPROM.write(fieldstartaddress, totalValidCharacters);

  EEPROM.commit();



  return true;
}



String getEEPROMData(int fieldstartaddress) {
  // The event name is a string whose length varies
  //The eeprom location at the start address holds the length of the data

  int data_length = EEPROM.read(fieldstartaddress);
  //Serial.println(data_length);
  int dataStartAddress = fieldstartaddress + 1;
  int dataEndAddress = dataStartAddress + data_length;

  String dataString;

  for (int addr = 0; addr < data_length; addr++)
  {
    char characterInEEPROM = EEPROM.read(addr + dataStartAddress);
    if (allowedCharacter(characterInEEPROM))
    {
      dataString = dataString + characterInEEPROM;
    }

  }


  return dataString;
}

// We wish to limit the characters to be within the ascii table

bool allowedCharacter(char questionableCharacter)
{
  if (questionableCharacter > ALLOWED_CHARACTER_UPPER_VALUE)
  {
    return false;
  }
  if (questionableCharacter < ALLOWED_CHARACTER_LOWER_VALUE)
  {
    return false;
  }
  return true;
}

char getHEXValue(uint8_t value)
{
  if (value < 10)
  {

    return 48 + value;
  }
  else
  {

    return 55 + value;
  }
}

boolean SetupMode = false;
boolean HTTPActionToBeInitiated=false;
const byte ACTION_TRIGGER_PIN = 2;
const byte SETUP_MODE_PIN = 0;
WiFiManager wifiManager;


void ICACHE_RAM_ATTR handleInterruptRising();
void ISRHTTPActionTrigger()
{
  detachInterrupt(digitalPinToInterrupt(ACTION_TRIGGER_PIN));
  HTTPActionToBeInitiated=true;
  Serial.println("Interrupted");

}

void ICACHE_RAM_ATTR ISRSetupMode();
void ISRSetupMode()
{
  detachInterrupt(digitalPinToInterrupt(SETUP_MODE_PIN));
  uint32_t startTime=millis();
  while(millis()-startTime<3000)
  {
    
  }
  if(digitalRead(SETUP_MODE_PIN)==LOW)
  {
    SetupMode=true;
    Serial.println("Going into Setup Mode");
  }
  

}









void setup() {

  /* We wish to evaluate how the reset happened
    /* It is possible we could reach here either by a loss of power or actual reset */
  /* Loss of Power should not lead to a notification being sent */
  pinMode(ACTION_TRIGGER_PIN, INPUT);
  pinMode(SETUP_MODE_PIN, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(ACTION_TRIGGER_PIN), ISRHTTPActionTrigger, RISING);
  attachInterrupt(digitalPinToInterrupt(SETUP_MODE_PIN), ISRSetupMode, FALLING);

  rst_info *resetInfo;

  resetInfo = ESP.getResetInfoPtr();
  int reasonCode = (*resetInfo).reason;

  Serial.println(reasonCode);
  

 


  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(512);

   // Lets look at the data that exists in the EEPROM
  s_ifttt_event = String(getEEPROMData(IFTTT_EVENT_START_ADDRESS));
  s_ifttt_token = String(getEEPROMData(IFTTT_TOKEN_START_ADDRESS));



  

 
  

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);



  
  wifiManager.setConfigPortalTimeout(60);

  

  
  





}

void ConnectToWifi()
{
 

  WiFiManagerParameter IFTTTEventName("ifttt_event", "ifttt_event", s_ifttt_event.c_str(), 100);

  WiFiManagerParameter IFTTTToken("ifttt_token", "ifttt_token", s_ifttt_token.c_str(), 100);

  //add all your parameters here
  wifiManager.addParameter(&IFTTTEventName);
  wifiManager.addParameter(&IFTTTToken);
  
  if (!wifiManager.autoConnect(AP_SSID, AP_PASSWORD)) {
    Serial.println("failed to connect and hit timeout");    
  }

  //if you get here you have connected to the WiFi
  Serial.println("Connected to AP");

  if (shouldSaveConfig)
  {
    //read updated parameters
    c_ifttt_event = IFTTTEventName.getValue();
    c_ifttt_token = IFTTTToken.getValue();

    //We save the Data we recieved in EEPROM.
    saveDataInEEPROM(IFTTT_TOKEN_START_ADDRESS, c_ifttt_token);
    saveDataInEEPROM(IFTTT_EVENT_START_ADDRESS, c_ifttt_event);
  }
}

bool sendIFTTTRequest()
{
  uint8_t mac[MAC_LENGTH];
  WiFi.macAddress(mac);
  String MACString = "";
  uint8_t i;
  for (i = 0; i < MAC_LENGTH; i++)
  {
    uint8_t MSN = (mac[i] & 0xF0) >> 4;
    uint8_t LSN = (mac[i] & 0x0F);

    uint8_t current_index = i << 1; //Multiply by 2
    //Serial.println(current_index);
    MACString = MACString + getHEXValue(MSN);
    MACString = MACString + getHEXValue(LSN);


  }





  HTTPClient http;

  String URL = "http://maker.ifttt.com/trigger/IFTTT_EVENT/with/key/IFTTT_TOKEN?value1=VALUE_1";
  //String IFTTT_EVENT(ifttt_event);
  //String IFTTT_TOKEN(ifttt_token);
  //String MACString(MACResult);
  URL.replace("IFTTT_EVENT", s_ifttt_event);
  URL.replace("IFTTT_TOKEN", s_ifttt_token);

  MD5Builder md5;
  md5.begin();
  md5.add(MACString);
  md5.calculate();

  URL.replace("VALUE_1", md5.toString());



  Serial.println(URL);

  http.begin(URL);

  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode == 200)// The IFTTT Request which went through
  {
    return true;
  }
  else
  {
    return false;
  }
}



void loop() {
  if (HTTPActionToBeInitiated)
  {
    delay(5000);// Wait for the Input to settle
    ConnectToWifi();
    while(sendIFTTTRequest()) //Make the HTTP Request. repeat until you get a success
    {
        ESP.restart();// Restart the ESP
    }
  }

  if (SetupMode)
  {
    delay(1000);
    wifiManager.resetSettings();
    ConnectToWifi();   
    ESP.restart();// Restart the ESP
  }


}
