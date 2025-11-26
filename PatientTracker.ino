#include <stdio.h>
#include <math.h>
#include <WiFi101.h>
#include <Wire.h>
#include <SPI.h>
#include <TinyScreen.h>
#include "BMA250.h"

#define SerialMonitorInterface SerialUSB //Debugging
//Define constants
#define DEVICE_ID "Patient 1"
#define SSID "PatientTracker"
#define NETWORK_KEY "P@ssword123"
#define lossExponent 2.3
//Init libraries
TinyScreen display = TinyScreen(TinyScreenPlus);
WiFiClient client;
BMA250 accel;
//Define variables
int oneMeterCalibration;
double distance;
double filteredRSSI;
const double alpha = 0.3;
bool decision;
bool calibrate = true;
bool standby = false;
bool alert = false;
bool fallDetected = false;
char data[100];
//Variables for RSSI Filtering
#define MEDIAN_WINDOW 5
long rssiBuf[MEDIAN_WINDOW];
int rssiIndex = 0;
int rssiCount = 0;

double smoothRSSI = -80;  
bool smoothInit = false;

//Asymmetric smoothing factors
const double alphaRise = 0.35;   // stronger signal -> follow quickly
const double alphaFall = 0.08;   // weaker signal -> follow slowly

//Distance rate limiting
double reportedDistance = -1;
double maxSpeed = 2.0;  // meters per second (limit)
unsigned long lastTimeMs = 0;
//Server info to communicate with host
const char* serverIP = "192.168.137.1";
const int serverPort = 8080;


void setup() {
  SerialMonitorInterface.begin(9600); //Debugging
  Wire.begin();
  //Init accelerometer
  accel.begin(BMA250_range_2g, BMA250_update_time_16ms);
  //Init display
  display.begin();
  display.setBrightness(10);
  display.setFont(thinPixel7_10ptFontInfo);
  //Init Wifi module
  WiFi.setPins(8, 2, A3, -1);
  WiFi.lowPowerMode();
  //Loop until connected
  while (WiFi.status() != WL_CONNECTED)
  {
    screenPrint("Connecting...");
    WiFi.begin(SSID, NETWORK_KEY);
  }
  screenPrint("WiFi Connected");
  delay(3000);
  //Loop to calibrate 1 meter distance for RSSI-distance conversion
  while (calibrate)
  {
    decision = calibrationDecision();
    delay(1000);
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
    delay(1000);
  }
}

void loop() {
  long rssi = WiFi.RSSI();
  distance = distanceCalculation(rssi);
  //Debugging
  SerialMonitorInterface.println("RSSI Value:");
  SerialMonitorInterface.println(rssi);
  SerialMonitorInterface.println("This is the calculated distance:");
  SerialMonitorInterface.println(distance);

  if (distance < 10.0)
  {
    fallDetection();
    handleFall();
    alert = false;
    while (!standby)
    {
      delay(100);
      display.clearScreen();
      display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Green);
      display.fontColor(TS_8b_White, TS_8b_Green);
      display.setCursor(48 - (display.getPrintWidth(DEVICE_ID)/2), 15);
      display.print(DEVICE_ID);
      display.setCursor(48 - (display.getPrintWidth("STANDBY MODE")/2), 30);
      display.print("STANDBY MODE");

      standby = true;
    }
  }
  else if (distance > 10.0)
  {
    fallDetection();
    handleFall();
    standby = false;
    while (!alert)
    {
      delay(100);
      display.clearScreen();
      display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Red);
      display.fontColor(TS_8b_White, TS_8b_Red);
      display.setCursor(48 - (display.getPrintWidth(DEVICE_ID)/2), 15);
      display.print(DEVICE_ID);
      display.setCursor(48 - (display.getPrintWidth("ALERTING STAFF")/2), 30);
      display.print("ALERTING STAFF");

      alert = true;
    }
    
    
    //Function to send information to host
    if (client.connect(serverIP, serverPort))
    {
      snprintf(data, sizeof(data), "device=%s;distance=%.0f;LEAVING", DEVICE_ID, distance);

      client.println("POST / HTTP/1.1");
      client.println("Host: windows");
      client.println("Content-Type: text/plain");
      client.print("Content-Length: ");
      client.println(strlen(data));
      client.println();
      client.print(data);
      
      delay(1000);
    }
    else 
    {
      SerialMonitorInterface.println("Failed to connect."); //Debugging
    }
    client.stop();
  }
}
//Function to convert RSSI to distance
long medianFilter() {
  long temp[MEDIAN_WINDOW];
  int count = rssiCount;

  for (int i = 0; i < count; i++)
    temp[i] = rssiBuf[i];

  // Simple insertion sort
  for (int i = 1; i < count; i++) {
    long v = temp[i];
    int j = i;
    while (j > 0 && temp[j - 1] > v) {
      temp[j] = temp[j - 1];
      j--;
    }
    temp[j] = v;
  }

  return temp[count/2];
}

double distanceCalculation(long rssi) {
  //Insert RSSI sample into median buffer
  rssiBuf[rssiIndex] = rssi;
  rssiIndex = (rssiIndex + 1) % MEDIAN_WINDOW;
  if (rssiCount < MEDIAN_WINDOW) rssiCount++;

  long medRSSI = medianFilter();  // filtered RSSI (removes spikes)
  //Asymmetric EMA smoothing (slow when RSSI drops)
  if (!smoothInit) 
  {
    smoothRSSI = medRSSI;
    smoothInit = true;
  } 
  else 
  {
    double alpha = (medRSSI > smoothRSSI) ? alphaRise : alphaFall;
    smoothRSSI = alpha * medRSSI + (1.0 - alpha) * smoothRSSI;
  }
  //Convert filtered RSSI to distance
  double exponent = (oneMeterCalibration - smoothRSSI) / (10.0 * lossExponent);
  double rawDistance = pow(10, exponent);

  if (rawDistance < 0.1) rawDistance = 0.1;
  if (rawDistance > 50.0) rawDistance = 50.0;
  //Rate-limited distance (smooth output)
  unsigned long now = millis();
  double dt = (lastTimeMs == 0) ? 0.1 : (now - lastTimeMs) / 1000.0;
  lastTimeMs = now;

  if (reportedDistance < 0) 
  {
    reportedDistance = rawDistance;
    return rawDistance;
  }
  double maxDelta = maxSpeed * dt;
  double diff = rawDistance - reportedDistance;
  if (fabs(diff) > maxDelta)
    diff = (diff > 0) ? maxDelta : -maxDelta;

  reportedDistance += diff;

  return reportedDistance;
}

//Function to print text on the TinyScreen
void screenPrint(char* t) {
  display.clearScreen();
  int width = display.getPrintWidth(t);
  display.setCursor(48-(width/2), 10);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print(t);
  delay(1000);
}
//
//Function to process if user wants to calibrate device
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
      screenPrint("Loading...");
      return false;
    }
    else if (display.getButtons(TSButtonUpperRight))
    {
      screenPrint("Loading...");
      return true;
    }
  }
}
//Function to process the calibration, assigning the RSSI at a 1 meter distance for calculating actual distance
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

void fallDetection() {
  accel.read();

  float ax = accel.X;
  float ay = accel.Y;
  float az = accel.Z;
  //Convert raw values to g-force
  float gx = ax / 256.0;
  float gy = ay / 256.0;
  float gz = az / 256.0;

  float magnitude = sqrt(gx*gx + gy*gy + gz*gz);

  if (magnitude > 2.5 && !fallDetected)
  {
    fallDetected = true;
    if (client.connect(serverIP, serverPort))
    {
      snprintf(data, sizeof(data), "device=%s;distance=%.0f;FALLEN", DEVICE_ID, distance);

      client.println("POST / HTTP/1.1");
      client.println("Host: windows");
      client.println("Content-Type: text/plain");
      client.print("Content-Length: ");
      client.println(strlen(data));
      client.println();
      client.print(data);
      
      delay(1000);
    }
    else 
    {
      SerialMonitorInterface.println("Failed to connect."); //Debugging
    }
    client.stop();
  }
  else
  {
    fallDetected = false;
  }
}

void handleFall() {
  if (!fallDetected)
  {
    return;
  }

  delay(100);
  display.clearScreen();
  display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Red);
  display.fontColor(TS_8b_White, TS_8b_Red);
  display.setCursor(48 - (display.getPrintWidth(DEVICE_ID)/2), 10);
  display.print(DEVICE_ID);
  display.setCursor(48 - (display.getPrintWidth("HAS FALLEN")/2), 25);
  display.print("HAS FALLEN");
  display.setCursor(48 - (display.getPrintWidth("Press to Reset")/2), 40);
  display.print("Press to Reset");

  while(true)
  {
    if (display.getButtons())
    {
      break;
    }
  }
  
  standby = false;
  alert = false;
  fallDetected = false;
  delay(300);
}
