#include <stdio.h>
#include <math.h>
#include <WiFi101.h>
#include <TinyScreen.h>

#define SerialMonitorInterface SerialUSB
#define SSID "PatientTracker"
#define NETWORK_KEY "P@ssword123"
#define oneMeterCalibration -60.00
#define lossExponent 2.7

TinyScreen display = TinyScreen(TinyScreenPlus);

double distance;
char* strDistance;

void setup() {
  SerialMonitorInterface.begin(9600);
  //Init display
  display.begin();
  display.setBrightness(10);
  //Init Wifi module
  WiFi.setPins(8, 2, A3, -1);
  WiFi.begin(SSID, NETWORK_KEY);
  //Loop until connected
  while (WiFi.status() != WL_CONNECTED)
  {
    screenPrint("Connecting...");
    delay(1000);
  }
  screenPrint("WiFi Connected");
  delay(2000);
}

void loop() {
  long rssi = WiFi.RSSI();
  distance = distanceCalculation(rssi);
  SerialMonitorInterface.println("This is the distance:");
  SerialMonitorInterface.println(distance);
  sprintf(strDistance, "%.2f meters", distance);
  screenPrint(strDistance);
  delay(3000);
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

double distanceCalculation(long rssi) {
  double distance;
  double exponent;
  exponent = (rssi-oneMeterCalibration)/((-10)*(lossExponent));
  distance = pow(10, exponent);
  return distance;
}