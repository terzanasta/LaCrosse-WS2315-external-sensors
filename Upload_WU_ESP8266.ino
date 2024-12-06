/*
Receive Weather Data through serial port and upload to Weather Underground throug WiFi */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <farmerkeith_BMP280.h>
#include <FS.h>  //Include File System Headers

const char* ssid[] = { "SSID1", "SSID2" };              //change the SSID
const char* password[] = { "password1", "password2" };  //change the passwords

const char* host = "My_Weather_Station";  // DNS name on local network

const char* R_filename = "/rain.txt";

bmp280 bmp0;

WiFiUDP ntpUDP;

ESP8266WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
ESP8266HTTPUpdateServer httpUpdater;

HTTPClient httpclient;
WiFiClient client;

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 7200000);
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).

///////////////Weather Umderground////////////////////////
char WEBPAGE[] = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?";
char ID[] = "your WU ID";
char PASSWORD[] = "your WU password";
String resp = "";
/////////////////////////////////////////////////////////
float temperaturef, temperature, temperatureIN;
float humidity;
float dewpoint;
float windspeedf, windspeed;
float windgustf, windgust = 0, WSwindGust;
float windirection, windirectionf;
float windchill;
float barometerf, barometer;
float sunPower;
int UV;
float rainf, rain, rainRate = 0, WSrain;
float WSav, WDav;
float rainhourf, rainhour = 0;
float rain24hourf, rain24hour = 0;
int Hour1, Hour24, WScount, WDcount;
boolean once1H;
boolean reset24H = true;
bool firstT = true, firstH = true, firstR = true, serialComplete = false;
unsigned long xronos = 0;
unsigned long WUxronos = 0;
String inputString = "";
String valid = "", RFread = "";
long Tm, Hm, Wsm, Wdm, Wgm, Rm;
long Tpm, Hpm, Wspm, Wdpm, Wgpm, Rrate;

void setup() {
  Serial.begin(57600);

  WiFi.mode(WIFI_STA);
  connectToWiFi();

  MDNS.begin(host);
  httpUpdater.setup(&server);

  server.on("/", handleRoot);

  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });

  server.begin();

  MDNS.addService("http", "tcp", 80);

  if (SPIFFS.begin()) {
    Serial.println("SPIFFS Initialize....ok");
  } else {
    Serial.println("SPIFFS Initialization...failed");
  }

  /*
  if (SPIFFS.format())
  {
    Serial.println("File System Formated");
  }
  else
  {
    Serial.println("File System Formatting Error");
  }
*/

  Wire.begin(0, 2);  //SDA:0, SCL:2
  delay(100);
  bmp0.begin(2, 5, 3, 7, 0, 0);  //2 samples fot T, 16 samples for Pr, 3 continuous mode, 4sec standby time, no filter, I2C no SPI
  delay(100);
  barometer = bmp0.readPressure();
  barometer = barometer / pow((1 - 32.00 * 0.0000225577), 5.25588);  //height at 32 meters, almost 7,888 meters per Hpa
  temperatureIN = bmp0.readTemperature();

  WSav = WDav = 0;
  WScount = WDcount = 0;
  Tm = millis();
  Hm = millis();
  Wsm = millis();
  Wdm = millis();
  Wgm = millis();
  Rm = millis();
  Tpm = Hpm = Wspm = Wdpm = Wgpm = Rrate = 0;
  timeClient.update();
  Hour1 = timeClient.getHours();
  Hour24 = timeClient.getDay();
  WUxronos = millis();
  xronos = millis();  //for testing

  if (SPIFFS.exists(R_filename)) {
    readfile();
    Serial.println(rain);
  }
}
//******************************************************************************
void loop() {

  server.handleClient();
  MDNS.update();

  if (millis() - xronos > 10) {  //check at 10 msec
    if (Serial.available() > 0)
      readSerial();
    xronos = millis();
  }
  if (millis() - WUxronos > 60000) {  //send to WU every minute
    timeClient.update();
    getPressure();
    weatherpage();  //format for WU, construct the Web Page and send to WU
    checkRain();
    rainRate = 0;  //reset rainRate, keep maximum for every WU upload
    windgust = 0;  //reset Wind gust for every WU update
    WUxronos = millis();
  }
}
//******************************************************************************
void readSerial() {
  while (Serial.available()) {
    inputString = Serial.readStringUntil('\n');
  }
  RFread = inputString;
  decodeSerial();
}
//******************************************************************************
void decodeSerial() {
  int Index1 = 0, Index2 = 0;
  float variable;
  String serialString;
  char chr1, chr2;

  valid = "OK";
  chr1 = inputString.charAt(0);
  chr2 = inputString.charAt(1);
  Index1 = inputString.indexOf("=");
  Index2 = inputString.indexOf("!");

  serialString = inputString.substring(Index1 + 2);  //, inputString.length());
  variable = serialString.toFloat();

  if (variable < 0) return;
  switch (chr1) {
    case 'T':                                     //T
      if (variable > -20.0 && variable < 50.0) {  //Temperature between -15 and 50 Celcius
        temperature = variable;
        Tpm = (millis() - Tm) / 1000;
        Tm = millis();
      }
      break;
    case 'H':                                //H
      if (variable > 10 && variable < 99) {  //Temperature between 10 and 99 %
        humidity = variable;
        Hpm = (millis() - Hm) / 1000;
        Hm = millis();
      }
      break;
    case 'W':  //W
      if (chr2 == 's') {
        if (variable <= 40 && variable >= 0) {  //40 m/s = 144 km/h, Wind limit
          windspeed = variable * 3.6;           //convert m/s to km/h weather station sends m/s
          WSav += windspeed;
          WScount++;
          Wspm = (millis() - Wsm) / 1000;
          Wsm = millis();
        }
        if (windgust <= windspeed) windgust = windspeed;  //max speed as Gust
      }
      if (chr2 == 'g') {
        if (variable <= 33) {
          WSwindGust = variable * 3.6;  //Wind Gust from Weather Station (not always accurate)
          Wgpm = (millis() - Wgm) / 1000;
          Wgm = millis();
        }
      }
      if (chr2 == 'd') {
        if (variable <= 360 && variable >= 0) {
          windirection = variable;  //d
          WDav += windirection;
          WDcount++;
          Wdpm = (millis() - Wdm) / 1000;
          Wdm = millis();
        }
      }
      break;
    case 'R':  //R
      if (variable > rain) {
        writefile(variable);
      }
      WSrain = variable;
      rainhour = rainhour + (variable - rain);
      rain24hour = rain24hour + (variable - rain);
      Rrate = millis() - Rm;
      Rm = millis();
      rainRate = (variable - rain) * (3600000.0 / Rrate);  //mm/hour
      rain = variable;
      break;
    default:
      break;
  }
}
//******************************************************************************
void checkRain() {
  if (Hour1 != timeClient.getHours()) {
    rainhour = 0;
    Hour1 = timeClient.getHours();
  }
  if (Hour24 != timeClient.getDay()) {
    rain24hour = 0;
    Hour24 = timeClient.getDay();
  }
}
//**************************************************************

void send2serial() {
  Serial.print("OUT-T : ");
  Serial.println(temperature, 1);
  Serial.print("OUT-H : ");
  Serial.println(humidity, 1);
  Serial.print("DewPoint : ");
  Serial.println(dewpoint, 1);
  Serial.print("WindSpeed : ");
  Serial.println(windspeed, 1);
  Serial.print("WindGust : ");
  Serial.println(windgust, 1);
  Serial.print("WindDir : ");
  Serial.println(windirection, 1);
  Serial.print("Pressure : ");
  Serial.println(barometer, 1);
  Serial.print("Rain rate : ");
  Serial.println(rainRate, 1);
  Serial.print("Rain 1H : ");
  Serial.println(rainhour, 1);
  Serial.print("Rain 24H : ");
  Serial.println(rain24hour, 1);
}
//******************************************************************************
void handleRoot() {
  String tmp;
  //timeClient.update();
  getPressure();
  tmp = "HTTP/1.1 200 OK";
  tmp += "Content-Type: text/html";
  tmp += ("Connection: close");
  tmp += ("Refresh: 60");
  tmp += ("<!DOCTYPE html>");
  tmp += ("<html xmlns='http://www.w3.org/1999/xhtml'>");
  tmp += ("<head>\n<meta charset='UTF-8'>");
  tmp += ("<title>My Weather Station</title>");
  tmp += ("</head>\n<body bgcolor='#000318'>");
  tmp += ("<div style='margin: auto; padding: 5px; text-align: center; font-family: Impact, Charcoal, sans-serif; color: #fff'>");
  tmp += ("<H1>My Weather Station</H1>");
  tmp += ("<div style='font-size: 1.0em;'>");
  tmp += (timeClient.getFormattedTime()) + "  " + valid;
  tmp += ("</b><br />");
  tmp += ("Raw data = ");
  tmp += RFread;
  tmp += ("</b><br />");
  tmp += ("Temperature = ");
  tmp += (float)temperature;
  tmp += ("</b><br />");
  tmp += ("Temperature IN= ");
  tmp += (float)temperatureIN;
  tmp += ("</b><br />");
  tmp += ("Humidity = ");
  tmp += (float)humidity;
  tmp += ("</b><br />");
  tmp += ("Wind Speed = ");
  tmp += (float)windspeed;
  tmp += ("</b><br />");
  tmp += ("Wind Gust = ");
  tmp += (float)windgust;
  tmp += ("</b><br />");
  tmp += ("Wind Direction = ");
  tmp += (float)windirection;
  tmp += ("</b><br />");
  tmp += ("Daily Rain = ");
  tmp += (float)rain24hour;
  tmp += ("</b><br />");
  tmp += ("Last hour Rain = ");
  tmp += (float)rainhour;
  tmp += ("</b><br />");
  tmp += ("Rain rate = ");
  tmp += (float)rainRate;
  tmp += ("</b><br />");
  tmp += ("Rain from file = ");
  tmp += (float)rain;
  tmp += ("</b><br />");
  tmp += ("Pressure = ");
  tmp += (float)barometer;
  tmp += ("</b><br />");
  tmp += ("</b><br />");
  tmp += ("WS-2315 rain = ");
  tmp += (float)WSrain;
  tmp += ("</b><br />");
  tmp += ("WS-2315 Gust = ");
  tmp += (float)WSwindGust;
  tmp += ("</b><br />");
  tmp += ("</b><br />");
  tmp += ("T timming = ");
  tmp += Tpm;
  tmp += ("</b><br />");
  tmp += ("H timming = ");
  tmp += Hpm;
  tmp += ("</b><br />");
  tmp += ("Speed timming = ");
  tmp += Wspm;
  tmp += ("</b><br />");
  tmp += ("Direction timming = ");
  tmp += Wdpm;
  tmp += ("</b><br />");
  tmp += ("Gust timming = ");
  tmp += Wgpm;
  tmp += ("</b><br />");
  tmp += ("Rain timming = ");
  tmp += (float)Rrate / 1000.0;
  tmp += ("</b><br />");
  tmp += ("WU_response = ");
  tmp += resp;
  tmp += ("</div>");
  tmp += ("");
  tmp += ("</div>");
  tmp += ("</body>\n</html>");
  server.send(200, "text/html", tmp);
}
//*****************************************************
double dewPoint(double tempf, double humidity)  //Calculate dew Point
{
  double A0 = 373.15 / (273.15 + tempf);
  double SUM = -7.90298 * (A0 - 1);
  SUM += 5.02808 * log10(A0);
  SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / A0))) - 1);
  SUM += 8.1328e-3 * (pow(10, (-3.49149 * (A0 - 1))) - 1);
  SUM += log10(1013.246);
  double VP = pow(10, SUM - 3) * humidity;
  double T = log(VP / 0.61078);
  return (241.88 * T) / (17.558 - T);
}
//******************************************************************************
void formatWU() {

  temperaturef = temperature * 1.8 + 32;  //Farehnite
  //insideTemp = insideTemp * 1.8 + 32; //Farehnite

  dewpoint = (float)dewPoint(temperaturef, humidity);  // Farehnite

  windspeedf = WSav / WScount * 0.6213712;  //mph
  windgustf = windgust * 0.6213712;         //mph
  windirectionf = WDav / WDcount;
  rainf = rain / 25.4;  //inches
  rainhourf = rainhour / 25.4;
  rain24hourf = rain24hour / 25.4;
  barometerf = barometer * 0.02952998751;  //mbar to inch Hg
  WSav = WDav = 0;
  WScount = WDcount = 0;
}

//******************************************************************************
void weatherpage() {
  //WiFiClient client;

  formatWU();
  String cmd = WEBPAGE;
  cmd += "ID=";
  cmd += ID;
  cmd += "&PASSWORD=";
  cmd += PASSWORD;
  cmd += "&dateutc=now";
  cmd += "&winddir=";
  cmd += windirection;
  cmd += "&windspeedmph=";
  cmd += windspeedf;
  cmd += "&windgustmph=";
  cmd += windgustf;
  cmd += "&tempf=";
  cmd += temperaturef;
  cmd += "&dewptf=";
  cmd += dewpoint;
  cmd += "&humidity=";
  cmd += humidity;
  cmd += "&baromin=";
  cmd += barometerf;
  cmd += "&rainin=";
  cmd += rainhourf;
  cmd += "&dailyrainin=";
  cmd += rain24hourf;
  cmd += "&softwaretype=ESP8266&action=updateraw";

  HTTPClient http;
  if (http.begin(client, cmd)) {
    int httpCode = http.GET();
    resp = http.getString();
  }
  http.end();

  //delay(2000);
}
//****************************************************************

void getPressure() {
  barometer = bmp0.readPressure();
  barometer = barometer / pow((1 - 32.00 * 0.0000225577), 5.25588);  //3HP height at 185 meters, almost 7,888 meters per Hpa
  temperatureIN = bmp0.readTemperature();
}

//****************************************************************************
void readfile() {
  //Read File data
  String str;
  File f = SPIFFS.open(R_filename, "r");
  if (f) {
    str = f.readString();
    rain = str.toFloat();
    f.close();  //Close file
    //Serial.println("File read OK");
  }
}
//****************************************************************************
void writefile(float accRain) {
  //Write File data
  File f = SPIFFS.open(R_filename, "w");
  if (f) {
    f.print(accRain, 2);
    f.close();  //Close file
  }
}
//****************************************************************************
void connectToWiFi() {
  int numNetworks = sizeof(ssid) / sizeof(ssid[0]);

  for (int i = 0; i < numNetworks; i++) {
    Serial.print("Connecting to ");
    Serial.println(ssid[i]);

    WiFi.begin(ssid[i], password[i]);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi");
      break;
    } else {
      Serial.println("\nConnection failed");
    }
  }
}
