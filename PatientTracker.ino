#include <stdio.h>
#include <math.h>
#include <WiFi101.h>
#include <Wire.h>
#include <SPI.h>
#include <TinyScreen.h>

#define SerialMonitorInterface SerialUSB //Debugging
//Define constants
#define BMA250_ADDR 0x18
#define BMA250_REG_CHIP_ID   0x00
#define BMA250_REG_PMU_RANGE 0x0F
#define BMA250_REG_PMU_BW    0x10
#define BMA250_REG_DATA_X_LSB 0x02
#define MOTION_DELTA_THRESHOLD 300.0

#define DEVICE_ID "Patient 1"
#define SSID "PatientTracker"
#define NETWORK_KEY "P@ssword123"
#define lossExponent 2.7
//Init libraries
TinyScreen display = TinyScreen(TinyScreenPlus);
WiFiClient client;
//Define variables
int oneMeterCalibration;
double distance;
bool decision;
bool calibrate = true;
bool standby = false;
bool alert = false;
char data[100];
int16_t ax, ay, az;
float prevMag = 0.0;
bool havePrevMag = false;
//Server info to communicate with host
const char* serverIP = "192.168.137.1";
const int serverPort = 8080;


void setup() {
  SerialMonitorInterface.begin(9600); //Debugging
  Wire.begin();
  //Init Accelerometer
  bmaInit();
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
  delay(5000);
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
  bool hardFall = detectFall();
  distance = distanceCalculation(rssi);
  //Debugging
  SerialMonitorInterface.println("RSSI Value:");
  SerialMonitorInterface.println(rssi);
  SerialMonitorInterface.println("This is the calculated distance:");
  SerialMonitorInterface.println(distance);

  if (distance < 5.0)
  {
    alert = false;
    while (!standby)
    {
      display.clearScreen();
      display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Green);
      display.fontColor(TS_8b_White, TS_8b_Green);
      display.setCursor(48 - (display.getPrintWidth("STANDBY MODE")/2), 20);
      display.print("STANDBY MODE");

      standby = true;
    }
    
  }
  else if (distance > 5.0)
  {
    standby = false;
    while (!alert)
    {
      display.clearScreen();
      display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Red);
      display.fontColor(TS_8b_White, TS_8b_Red);
      display.setCursor(48 - (display.getPrintWidth("ALERTING STAFF")/2), 20);
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
      
      delay(5000);
    }
    else 
    {
      SerialMonitorInterface.println("Failed to connect."); //Debugging
    }
    client.stop();
  }

  while (hardFall)
  {
    hardFall = resetFall();
    
    if (client.connect(serverIP, serverPort))
    {
      snprintf(data, sizeof(data), "device=%s;distance=%.0f;FALL", DEVICE_ID, distance);

      client.println("POST / HTTP/1.1");
      client.println("Host: windows");
      client.println("Content-Type: text/plain");
      client.print("Content-Length: ");
      client.println(strlen(data));
      client.println();
      client.print(data);
      
      delay(5000);
    }
    else 
    {
      SerialMonitorInterface.println("Failed to connect."); //Debugging
    }
    client.stop();
  }
}
//Function to convert RSSI to distance
double distanceCalculation(long rssi) {
  double distance;
  double exponent;
  exponent = (rssi-oneMeterCalibration)/((-10)*(lossExponent));
  distance = pow(10, exponent);
  return distance;
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

uint8_t bmaRead8(uint8_t reg) {
  Wire.beginTransmission(BMA250_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(BMA250_ADDR, (uint8_t)1);
  
  if (Wire.available()) 
  {
    return Wire.read();
  }
  return 0;
}

void bmaWrite8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BMA250_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void bmaInit() {
  // Check chip ID (BMA250 should report 0x03 or similar)
  uint8_t id = bmaRead8(BMA250_REG_CHIP_ID);

  SerialUSB.print("BMA250 chip ID: ");
  SerialUSB.println(id, HEX);

  // Set range to ±2g (0x03) – see BMA250 datasheet
  bmaWrite8(BMA250_REG_PMU_RANGE, 0x03);

  // Set bandwidth (low‑pass filter & output data rate). 0x0A ≈ 125 Hz.
  bmaWrite8(BMA250_REG_PMU_BW, 0x0A);

  delay(10);
}

void bmaReadAccel() {
  uint8_t buf[6];

  Wire.beginTransmission(BMA250_ADDR);
  Wire.write(BMA250_REG_DATA_X_LSB);
  Wire.endTransmission(false);
  Wire.requestFrom(BMA250_ADDR, (uint8_t)6);

  for (int i = 0; i < 6; i++) 
  {
    if (Wire.available())
    {
      buf[i] = Wire.read();
    }
    else
    {
      buf[i] = 0;
    }
  }

  // BMA250 outputs 10‑bit data: bits 7:0 in LSB, bits 9:8 in MSB (bits 1:0)
  int16_t x = (int16_t)((int16_t)((buf[1] << 8) | buf[0]) >> 6);
  int16_t y = (int16_t)((int16_t)((buf[3] << 8) | buf[2]) >> 6);
  int16_t z = (int16_t)((int16_t)((buf[5] << 8) | buf[4]) >> 6);

  // Sign‑extend 10‑bit values to 16‑bit
  if (x & 0x0200) x |= 0xFC00;
  if (y & 0x0200) y |= 0xFC00;
  if (z & 0x0200) z |= 0xFC00;

  ax = x;
  ay = y;
  az = z;
}

bool detectFall() {
  bool largeMotion = false;

  bmaReadAccel();
  float fx = (float)ax;
  float fy = (float)ay;
  float fz = (float)az;
  float mag = sqrt(fx * fx + fy * fy + fz * fz);
  float delta = 0.0;
  
  if (havePrevMag) 
  {
    delta = fabs(mag - prevMag);
    if (delta > MOTION_DELTA_THRESHOLD) 
    {
      largeMotion = true;
      SerialUSB.print("Large motion detected!  |Δa| = ");
      SerialUSB.println(delta);
    }
  } 
  else 
  {
    havePrevMag = true;
  }

  prevMag = mag;
  return largeMotion;
}

bool resetFall() {
  bool buttonPressed = false;
  bool hardFall = true;

  display.clearScreen();
  display.drawRect(0, 0, 96, 64, TSRectangleFilled, TS_8b_Red);
  display.fontColor(TS_8b_White, TS_8b_Red);
  display.setCursor(48 - (display.getPrintWidth("ALERT!")/2), 20);
  display.print("ALERT");
  display.setCursor(48 - (display.getPrintWidth("PATIENT FALLEN")/2), 30);
  display.print("PATIENT FALLEN");
  display.setCursor(48 - (display.getPrintWidth("Press to reset")/2), 40);
  display.print("Press to reset");

  while (!buttonPressed)
  {
    if (display.getButtons())
    {
      hardFall = false;
      return hardFall;
    }
  }
}
