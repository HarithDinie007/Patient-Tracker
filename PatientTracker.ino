#include <stdio.h>
#include <math.h>
#include <WiFi101.h>
#include <TinyScreen.h>

#define SerialMonitorInterface SerialUSB
#define SSID "PatientTracker"
#define NETWORK_KEY "P@ssword123"
#define lossExponent 2.7
//Init libraries
TinyScreen display = TinyScreen(TinyScreenPlus);
WiFiClient client;

int oneMeterCalibration;
double distance;
bool decision;
bool calibrate = true;

const char* serverIP = "192.168.137.1";
const int serverPort = 8080;

void setup() {
  SerialMonitorInterface.begin(9600); //debugging
  //Init display
  display.begin();
  display.setBrightness(10);
  //Init Wifi module
  WiFi.setPins(8, 2, A3, -1);
  WiFi.begin(SSID, NETWORK_KEY);
  WiFi.lowPowerMode();
  //Loop until connected
  while (WiFi.status() != WL_CONNECTED)
  {
    screenPrint("Connecting...");
  }
  screenPrint("WiFi Connected");
  delay(5000);
  
  while (calibrate)
  {
    decision = calibrationDecision();
    delay(2000);
    if (decision)
    {
      oneMeterCalibration = calibrationMode();
      calibrate = false;
    }
    else if (!decision)
    {
      oneMeterCalibration = -60;
      calibrate = false;
    }
  }
}

void loop() {
  long rssi = WiFi.RSSI();
  distance = distanceCalculation(rssi);
  SerialMonitorInterface.println(rssi);
  SerialMonitorInterface.println("This is the distance:");
  SerialMonitorInterface.println(distance);
  if (distance < 5.0)
  {
    screenPrint("Standby Mode");
  }
  else if (distance > 5.0)
  {
    screenPrint("Alerting staff");
    if (client.connect(serverIP, serverPort))
    {
      String data = "device=Patient_1;ALERT";

      client.println("POST / HTTP/1.1");
      client.println("Host: windows");
      client.println("Content-Type: text/plain");
      client.print("Content-Length: ");
      client.println(data.length());
      client.println();
      client.print(data);
    }
    else 
    {
      SerialMonitorInterface.println("Failed to connect.");
    }

    client.stop();
  }
  delay(3000);
}

double distanceCalculation(long rssi) {
  double distance;
  double exponent;
  exponent = (rssi-oneMeterCalibration)/((-10)*(lossExponent));
  distance = pow(10, exponent);
  return distance;
}

void screenPrint(char* t) {
  display.clearScreen();
  display.setFont(thinPixel7_10ptFontInfo);
  int width = display.getPrintWidth(t);
  display.setCursor(48-(width/2), 10);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print(t);
  delay(1000);
}

bool calibrationDecision() {
  bool buttonPressed = false;
  display.clearScreen();
  display.setCursor(0, 0);
  display.print("< No");
  display.setCursor(95 - display.getPrintWidth("Yes >"), 0);
  display.print("Yes >");
  display.setCursor(48 - (display.getPrintWidth("Calibrate?")/2), 10);
  display.print("Calibrate?");
  while (!buttonPressed)
  {
    if (display.getButtons(TSButtonUpperLeft))
    {
      return false;
    }
    else if (display.getButtons(TSButtonUpperRight))
    {
      return true;
    }
  }
}

int calibrationMode() {
  bool buttonPressed = false;
  int rssi;
  display.clearScreen();
  display.setCursor(48 - (display.getPrintWidth("Press at 1 meter")/2), 10);
  display.print("Press at 1 meter");
  while (!buttonPressed)
  {
    if (display.getButtons())
    {
      rssi = WiFi.RSSI();
      return rssi;
    }
  }
}