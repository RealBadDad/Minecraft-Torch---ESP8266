/*------------------------------------------------------------------------------
  27th Sept 2020
  Author: RealBadDad
  Platforms: NodeMCU 1.0 (ESP-12E Module) 
  ------------------------------------------------------------------------------
  Description:
  Basic WEB interface with the following features

  1) Hosted WiFi - Station
  2) Hard coded WiFi SSID and PASSWORD
  3) MDNS responder
  4) SPIFFS filing system
  5) Storage of settings in file system
  6) HTML web page in index.html
  7) Style sheet in style.css
  8) Websocket data passing
  9) JSON data passing structures to and from index.html
  10) OTA update system for embedded code

  C:\Users\username\AppData\Local\Temp

  ------------------------------------------------------------------------------
  License:
  Please see attached LICENSE.txt file for details.
  ------------------------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <NeoPixelBrightnessBus.h>

/* WIFI Interface settings                  */
/* Edit these for your Wifi Router settings */
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

/* MDNS name and responder                  */
/* Give your unit a name so that you dont   */
/* need to search for IP adresses           */
const char* host = "WilliamsMinecraft";
/*Final name will be http://WilliamsMinecraft.local/    */
MDNSResponder mdns;

/* HTTP Server and Websockets */
ESP8266WebServer httpServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

/* Update Server */
ESP8266HTTPUpdateServer httpUpdater;

/* LED hardware */
/* Make sure you alter these settings for   */
/* the hardware you are using               */
const uint16_t NoPixels = 9;
const uint8_t PixelPin = 2;
const uint8_t EnablePin = 2;

/* Customise the WEB page                   */
String Heading1 = "Williams";
String Heading2 = "Minecraft Torch";

/* Alter the settings for the type of LEDs you have fittted. */
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266DmaWs2812xMethod  > strip(NoPixels, PixelPin);

int SetRedLevel = 0;
int SetGreenLevel = 0;
int SetBlueLevel = 0;
int SetBrightnessLevel = 0;
int SetRateLevel = 0;
int SetStoreRequest = 0;
int SetONOFFRequest = 0;
int SetEffectType = 0;

/* Primary setup calls */
void setup(void)
{
  /*Setup for all Debugging*/
  Serial.begin(115200);
  Serial.println();

  /*WIFI Setup*/
  // Wifi Mode is a AP (access point) STA (Station)
  WiFi.mode(WIFI_STA);
  WiFi.hostname(host);
  Serial.print("Connecting ");
  // Connect to the local Wifi router with pre-defined SSID and password
  WiFi.begin(ssid, password);
  // Wait for the connection to be made
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  // WiFi connection is OK
  Serial.println ( "" );
  Serial.print ( "Connected to " ); Serial.println ( ssid );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );

  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    Serial.println("SPIFFS Started");
  }

  /* Setup MDNS*/
  if (mdns.begin(host, WiFi.localIP()))
  {
    Serial.println ( "MDNS responder started" );
  }

  httpUpdater.setup(&httpServer);
  // On root address handle the main web page
  httpServer.on ( "/", handleRoot );
  httpServer.on ( "/Enable", handleEnable);
  httpServer.on ( "/Disable", handleDisable);
  httpServer.on ( "/Store", handleStoreRequest);
  httpServer.on ( "/JsonCtrl", handleJSONRequest);
  // Route to load style.css file
  httpServer.onNotFound(handleWebRequests);

  //Handle page error
  //httpServer.onNotFound(handleNotFound);

  //Start Server
  httpServer.begin();

  mdns.addService("http", "tcp", 80);
  mdns.addService("ws", "tcp", 81);
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  // try every 5000ms again if connection has failed
  //webSocket.setReconnectInterval(5000);
  Serial.printf("HTTPServer ready! Open http://%s.local/update in your browser\n", host);

  LoadSettings();

  pinMode(EnablePin, OUTPUT);
  digitalWrite(EnablePin, HIGH);

  // This resets all the neopixels to an off state
  strip.Begin();
  strip.Show();
}

/* Server  */
void handleEnable()
{
  SetONOFFRequest = 1;
  Serial.println("Enable");
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}
void handleDisable()
{
  SetONOFFRequest = 0;
  Serial.println("Disable");
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}

void handleJSONRequest()
{
    String data = httpServer.arg("plain");
    Serial.print("From http st - ");
    Serial.println(data);
    
    StaticJsonDocument<200> jsonBuffer;
    deserializeJson(jsonBuffer, data);
    
    String jsonStr = "";
    serializeJson(jsonBuffer, jsonStr);
    Serial.print("From http json - ");
    Serial.println(jsonStr);

    if(!jsonBuffer["RedLevel"].isNull())
    {
      SetRedLevel = (int)jsonBuffer["RedLevel"];
    }
    if(!jsonBuffer["GreenLevel"].isNull())
    {
      SetGreenLevel = (int)jsonBuffer["GreenLevel"];
    }
    if(!jsonBuffer["BlueLevel"].isNull())
    {
      SetBlueLevel = (int)jsonBuffer["BlueLevel"];
    }
    if(!jsonBuffer["Brightness"].isNull())
    {
      SetBrightnessLevel = (int)jsonBuffer["Brightness"];
    }
    if(!jsonBuffer["Rate"].isNull())
    {
      SetRateLevel = (int)jsonBuffer["Rate"];
    } 
    if(!jsonBuffer["EffectType"].isNull())
    {
      SetEffectType = (int)jsonBuffer["EffectType"];
    } 
    if(!jsonBuffer["UserReqOnOFF"].isNull())
    {
      SetONOFFRequest = (int)jsonBuffer["UserReqOnOFF"];
    }
    
    //Update all other web pages that are attached.
    UpdateWebPage(false);
}



void handleStoreRequest()
{
  SetStoreRequest = 1;
  Serial.println("StoreRequested");  
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}


void handleWebRequests() {
  if (loadFromSpiffs(httpServer.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " NAME:" + httpServer.argName(i) + "\n VALUE:" + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  Serial.println(message);
}

bool loadFromSpiffs(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";
  else if (path.endsWith(".ttf")) dataType = "font/ttf";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (httpServer.hasArg("download")) dataType = "application/octet-stream";
  if (httpServer.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

/* Send the current settigns to the Web page */
void UpdateWebPage(bool InitialSettings)
{
  StaticJsonDocument<200> jsonBuffer;
  jsonBuffer["RedLevel"] = SetRedLevel;
  jsonBuffer["GreenLevel"] = SetGreenLevel;
  jsonBuffer["BlueLevel"] = SetBlueLevel;
  jsonBuffer["Brightness"] = SetBrightnessLevel;
  jsonBuffer["Rate"] = SetRateLevel;
  jsonBuffer["EffectType"] = SetEffectType;
  jsonBuffer["UserReqOnOFF"] = SetONOFFRequest;
  /* Additional non functional page data to setup names and haeadings on web page */
  if (InitialSettings == true)
  {
    jsonBuffer["Heading1"] = Heading1;
    jsonBuffer["Heading2"] = Heading2;
  }
  String jsonStr = "";
  serializeJson(jsonBuffer, jsonStr);
  Serial.print("To web page - ");
  Serial.println(jsonStr);
  /* Send JSON string over the websocket connection */
  webSocket.broadcastTXT(jsonStr);
}

/* Websocket has had an even. If Text has been received extract any known JSON data from it */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  StaticJsonDocument<200> jsonBuffer;

  if (type == WStype_TEXT)
  {
    deserializeJson(jsonBuffer, payload);

    String jsonStr = "";
    serializeJson(jsonBuffer, jsonStr);
    Serial.print("From web page - ");
    Serial.println(jsonStr);

    if(!jsonBuffer["RedLevel"].isNull())
    {
      SetRedLevel = (int)jsonBuffer["RedLevel"];
    }
    if(!jsonBuffer["GreenLevel"].isNull())
    {
      SetGreenLevel = (int)jsonBuffer["GreenLevel"];
    }
    if(!jsonBuffer["BlueLevel"].isNull())
    {
      SetBlueLevel = (int)jsonBuffer["BlueLevel"];
    }
    if(!jsonBuffer["Brightness"].isNull())
    {
      SetBrightnessLevel = (int)jsonBuffer["Brightness"];
    }
    if(!jsonBuffer["Rate"].isNull())
    {
      SetRateLevel = (int)jsonBuffer["Rate"];
    } 
    if(!jsonBuffer["EffectType"].isNull())
    {
      SetEffectType = (int)jsonBuffer["EffectType"];
    } 
    if(!jsonBuffer["UserReqOnOFF"].isNull())
    {
      SetONOFFRequest = (int)jsonBuffer["UserReqOnOFF"];
    }
       
    //Update all other web pages that are attached.
    UpdateWebPage(false);
  }
  /* Setup page on first connection */
  else if (type == WStype_CONNECTED)
  {
    UpdateWebPage(true);
  }
}

/* Store crretn light settings in a JSON string and stor this to a file called config.json */
void StoreSettings(void)
{
  StaticJsonDocument<200> jsonBuffer;

  /* Attempt to open config file for writing to */
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  else
  {
    /* File has been opened */
    jsonBuffer["RedLevel"] = SetRedLevel;
    jsonBuffer["GreenLevel"] = SetGreenLevel;
    jsonBuffer["BlueLevel"] = SetBlueLevel;
    jsonBuffer["Brightness"] = SetBrightnessLevel;
    jsonBuffer["Rate"] = SetRateLevel;
    jsonBuffer["EffectType"] = SetEffectType;
    jsonBuffer["UserReqOnOFF"] = SetONOFFRequest;

    /* Create Text strig of JSON data and write it to the file */
    String jsonStr = "";
    serializeJson(jsonBuffer, jsonStr);
    Serial.print("Writing to file - ");
    Serial.println(jsonStr);
    serializeJson(jsonBuffer, configFile);
  }

  /* Close any open file */
  configFile.close();
}

/* Load settings data from Config file */
void LoadSettings (void)
{
  StaticJsonDocument<200> jsonBuffer;

  if (SPIFFS.exists("/config.json"))
  {
    //file exists, reading and loading
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile)
    {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      deserializeJson(jsonBuffer, buf.get());

      String jsonStr = "";
      serializeJson(jsonBuffer, jsonStr);
      Serial.print("Reading from file - ");
      Serial.println(jsonStr);

      SetRedLevel = (int)jsonBuffer["RedLevel"];
      SetGreenLevel = (int)jsonBuffer["GreenLevel"];
      SetBlueLevel = (int)jsonBuffer["BlueLevel"];
      SetBrightnessLevel = (int)jsonBuffer["Brightness"];
      SetRateLevel = (int)jsonBuffer["Rate"];
      SetEffectType = (int)jsonBuffer["EffectType"];
      SetONOFFRequest = (int)jsonBuffer["UserReqOnOFF"];

    }
    else
    {
      Serial.println("Failed to open config file");
    }
  }
  else
  {
    Serial.println("No config file found");
  }
}


unsigned long previousMillis = 0;        // will store last time LED was updated
/* You can alter the brightness and the level
   and rate of flickering for the torch here */
#define  MaxBrightness    80
#define  MinBrightness    20
unsigned long delayMillis = 0;
uint8_t ledpointer = 0;
uint8_t colourpointer = 0;
uint8_t startposition = 0;
uint8_t lasteffect = 0;
uint8_t flashcount = 0;
int NewLevel=0;
                       
/* Flicker routine */
void LightingEffects(void)
{
  unsigned long currentMillis = millis();
  int position = 0;
  RgbColor ImmColour;
  
  /*Request to store current settings*/
  if (SetStoreRequest > 0)
  {
    SetStoreRequest = 0;
    StoreSettings();
    Serial.println("Settings Store activated");
  }

  /* Light is off*/
  if (SetONOFFRequest == 0)
  {
    if (ledpointer <= strip.PixelCount())
    {
      ledpointer = strip.PixelCount() + 10;
      delayMillis = 0;
      strip.ClearTo(RgbColor(0, 0, 0));
      strip.Show();
    }
  }
  
  /*Light is on*/
  else
  {
    if(SetEffectType>2)
    {
      delayMillis = SetRateLevel;
      if (currentMillis - previousMillis >= 400)
      {
        // save the last time you altered the LEDs
        previousMillis = currentMillis;

        if(colourpointer>0)
        {
          colourpointer=0;
          flashcount++;
          switch(SetEffectType)
          {   
            case 3: /*6--->DINNER - GREEN FLASHING ON & OFF SLOW*/  
              ImmColour = RgbColor(0, 255, 0);
            break;
    
            case 4: /*7--->BEDTIME - BLUE FLASHING ON & OFF SLOW */  
              ImmColour = RgbColor(0, 0, 255);
            break;
    
            case 5: /*8--->TOO LOUD - RED FLASHING ON & OFF FAST */  
              ImmColour = RgbColor(255, 0, 0);
            break;
      
            default:
              colourpointer = 0;
              startposition = 0;
              flashcount = 0;
              ledpointer = strip.PixelCount() ;
            break;
          }
        }
        else
        {
          colourpointer=1;
          ImmColour = RgbColor(0, 0, 0);
        }
        ledpointer = 0;
        while (ledpointer < strip.PixelCount())
        {
          strip.SetPixelColor(ledpointer, ImmColour);
          ledpointer++;
        }   
        strip.SetBrightness(100);
        if(flashcount > 10)
        {
          SetEffectType=lasteffect;
          colourpointer = 0;
          startposition = 0;
          flashcount = 0;
          ledpointer = strip.PixelCount();
          previousMillis = currentMillis;
          UpdateWebPage(false);
        }     
      }
      /* Update all LEDs */
      strip.Show();
    }
    else
    {
      flashcount = 0;
      if((lasteffect!=SetEffectType) || (ledpointer > strip.PixelCount()))
      {
        lasteffect=SetEffectType;
        colourpointer = 0;
        startposition = 0;
        ledpointer = strip.PixelCount();
        previousMillis = currentMillis;
      }

      if (currentMillis - previousMillis >= delayMillis)
      {
        // save the last time you altered the LEDs
        previousMillis = currentMillis;
        // update delay time if its been changed
        delayMillis = SetRateLevel;
        
        switch(SetEffectType)
        {
          case 0: /*0--->Solid*/
             ledpointer = 0;
             while (ledpointer < strip.PixelCount())
             {
                strip.SetPixelColor(ledpointer, RgbColor(SetBrightnessLevel*SetRedLevel/255, SetBrightnessLevel*SetGreenLevel/255, SetBrightnessLevel*SetBlueLevel/255)); 
                ledpointer++;
             }
             /* Delay to next  update */
            delayMillis = SetRateLevel; 
          break;
  
          case 1: /*1--->Rainbow Cycle - fading whole light through the colours*/
            ledpointer = 0;
            if (colourpointer < 0xFF)
            {
              colourpointer++;
            }
            else
            {
              colourpointer = 0;
            }
            while(ledpointer < strip.PixelCount())
            {
              strip.SetPixelColor(ledpointer, Wheel(colourpointer & 0xFF));
              ledpointer++; 
            }                
          break;
          
          case 2: /*2--->Flame - Flame / Fire effect*/     
            /* Calculate a new random brightness level */
            NewLevel = random(0,SetBrightnessLevel);
            /* Set Pixel to correct colour */
            strip.SetPixelColor(random(strip.PixelCount()), RgbColor(NewLevel*SetRedLevel/255, NewLevel*SetGreenLevel/255, NewLevel*SetBlueLevel/255));
        
            /* Delay to next flicker update */
            delayMillis = random(((255-SetRateLevel))); 
          break;
  
          default:
            colourpointer = 0;
            startposition = 0;
            ledpointer = strip.PixelCount() ;
          break;
        }
        /* set brightness of all leds */
       // strip.SetBrightness(SetBrightnessLevel);
      }
      
      /* Update all LEDs */
      strip.Show();
    } 
  }
}     


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
RgbColor Wheel(uint8_t WheelPos) 
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) 
  {
    return RgbColor(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) 
  {
    WheelPos -= 85;
    return RgbColor(0, WheelPos * 3, 255 - WheelPos * 3);
  } else 
  {
    WheelPos -= 170;
    return RgbColor(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

/* Server root web page */
void handleRoot()
{
  File file = SPIFFS.open("/index.html", "r");
  httpServer.streamFile(file, "text/html");
  file.close();
}

/* Main program loop */
void loop(void)
{
  webSocket.loop();
  mdns.update();
  httpServer.handleClient();
  LightingEffects();
}
