#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <NodeMcuFile.h>
#include <ThingSpeak.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

void sendData(float val_A, String& chNum_A, String& apiCode_A);
void handleRoot();
void handleNotFound();
void sendPage();

const char* host = "esp8266-plants-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

String channelNumber;
const String channelNumFile = "channelNum";

String logApiCode;
const String logApiCodeFile = "logApiKey";

bool readParamRes = false;

NodeMcuFile f;

WiFiClient thingSpeakClient;

//counter for loop
unsigned long g_loopCounter = 0;

char apiCodeBuff[50] = "";

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect("AutoConnectAP")) 
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("Connected");

  Serial.print(WiFi.localIP());

  Serial.println();

  httpServer.on ( "/", handleRoot );
  httpServer.onNotFound ( handleNotFound );

  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);

  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  
  Serial.println("Server started");

  ThingSpeak.begin(thingSpeakClient);

  //File system
  f.start();

  //Open log channel number
  bool tempRes1 = f.readFile(channelNumFile, channelNumber);

  //Open log API code
  bool tempRes2 = f.readFile(logApiCodeFile, logApiCode);

  if(tempRes1 && tempRes2)
  {
    readParamRes = true;
  }
}

void loop() 
{
  httpServer.handleClient();

  //increment counter
  g_loopCounter ++;
  
  //check the timeout
  if((g_loopCounter % 6000000) == 0)
  {
    int val = analogRead(0);
    sendData((float)val, channelNumber, logApiCode);
    g_loopCounter = 0;
  }
}

void sendData(float val_A, String& chNum_A, String& apiCode_A)
{ 
  if(!readParamRes)
    return;
    
  strcpy(apiCodeBuff, apiCode_A.c_str());

  int stat = ThingSpeak.writeField(chNum_A.toInt(), 1, val_A, apiCodeBuff);

  Serial.print("write status:");
  Serial.println(stat);
}

void handleRoot() 
{  
  if(httpServer.hasArg("CH_NUM") || httpServer.hasArg("CODE")) 
  {
    if(httpServer.hasArg("CH_NUM"))
    {
      channelNumber = httpServer.arg("CH_NUM");
      Serial.print("Channel:");
      Serial.println(channelNumber);
      f.saveFile(channelNumFile, channelNumber);
    }
    if(httpServer.hasArg("CODE"))
    {
      logApiCode = httpServer.arg("CODE");
      Serial.print("API code:");
      Serial.println(logApiCode);
      f.saveFile(logApiCodeFile, logApiCode);
    }
    sendPage();
  }
  else 
  {
    sendPage();
  }
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for ( uint8_t i = 0; i < httpServer.args(); i++ ) 
  {
    message += " " + httpServer.argName ( i ) + ": " + httpServer.arg ( i ) + "\n";
  }

  httpServer.send ( 404, "text/plain", message );
}

void sendPage()
{
  char temp[1000];
  
  snprintf ( temp, 1000,

    "<html>\
      <head>\
        <title>IOT Plant Webserver</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
        </head>\
        <body>\
          <h1>IOT Plant Webserver</h1>\
          <BR>\
          <FORM ACTION=\"http://%s\" method=get >\
          Channel number: %s\
          <BR>\
          <INPUT TYPE=TEXT NAME=\"CH_NUM\" VALUE=\"%s\" SIZE=\"25\" MAXLENGTH=\"50\"><BR>\
          <BR>\
          Write API: %s\
          <BR>\
          <INPUT TYPE=TEXT NAME=\"CODE\" VALUE=\"%s\" SIZE=\"30\" MAXLENGTH=\"50\"><BR>\
          <BR>\
          <INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Apply\">\
          <BR>\
          <A HREF=\"javascript:window.location.href = 'http://%s'\">Click to refresh the page</A>\
          </FORM>\
          <BR>\
        </body>\
      </html>",
  
      WiFi.localIP().toString().c_str(),
      channelNumber.c_str(),
      channelNumber.c_str(),
      logApiCode.c_str(),
      logApiCode.c_str(),
      WiFi.localIP().toString().c_str()
    );
//  Serial.println(temp);
  httpServer.send ( 200, "text/html", temp );
}


